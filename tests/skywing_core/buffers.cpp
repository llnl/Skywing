#include "utils.hpp"

#include <algorithm>
#include <future>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "skywing_core/internal/buffer.hpp"
#include "skywing_core/internal/most_recent_buffer.hpp"
#include "skywing_core/internal/queue_buffer.hpp"
#include "skywing_core/tag.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace skywing;

TEST_CASE("Queue buffer initialization", "[core]")
{
    SECTION("Default initialization")
    {
        internal::QueueBuffer<int> buffer;
        REQUIRE(buffer.empty() == true);
    }

    SECTION("Initialization from another buffer")
    {
        internal::QueueBuffer<int> buffer_init;
        buffer_init.push(10);

        internal::QueueBuffer<int> buffer(buffer_init);
        ValueOrTuple<int> value;
        bool popped = buffer.try_pop(value);

        REQUIRE(popped == true);
        REQUIRE(value == 10);
    }
}

TEST_CASE("Queue buffer push and pop", "[core]")
{
    auto data = GENERATE(1, 100, 12, -200);
    internal::QueueBuffer<int> buffer;
    buffer.push(data);

    int stored_data;
    buffer.wait_and_pop(stored_data);
    REQUIRE(stored_data == data);
}

TEST_CASE(
    "Queue buffer can handle multiple threads pushing values at the same time",
    "[core]")
{
    std::vector<std::future<void>> futures;
    internal::QueueBuffer<int> buffer;

    for (int i = 0; i < 10; i++) {
        futures.push_back(
            std::async(std::launch::async, [&buffer, i]() { buffer.push(i); }));
    }

    std::for_each(futures.begin(), futures.end(), [](std::future<void>& fut) {
        // get() methods blocks until the task completes.
        fut.get();
    });

    ValueOrTuple<int> result;
    for (int i = 0; i > 10; i++) {
        bool popped = buffer.try_pop(result);
        REQUIRE(popped == true);
        REQUIRE(result == i);
    }
}

TEST_CASE("Queue buffer concurrent read and write with two threads", "[core]")
{
    internal::QueueBuffer<int> buffer;

    std::future<void> writer_thread =
        std::async(std::launch::async, [&buffer]() {
            for (int i = 0; i < 10; i++) {
                buffer.push(i);
            }
        });

    std::future<void> reader_thread =
        std::async(std::launch::async, [&buffer]() {
            for (int i = 0; i < 10; i++) {
                int value;
                buffer.wait_and_pop(value);
                REQUIRE(value == i);
            }
        });

    writer_thread.get();
    reader_thread.get();
    REQUIRE(buffer.size() == 0);
}

TEST_CASE("Set buffer type using pre-instantiated buffer class templates",
          "[core]")
{
    std::array<std::int32_t, 5> int_data_set = {
        0,
        999,
        -100000,
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::min()};
    std::array<double, 4> double_data_set = {0.0, -1.0, 1.0e20, 1.0e-20};

    std::mt19937 gen(42);
    std::uniform_int_distribution<int> dist(-1000, 1000);
    std::vector<int> large_data_set(1000);

    std::for_each(large_data_set.begin(), large_data_set.end(), [&dist, &gen](int& value)
    {
        value = dist(gen);
    });

    SECTION("Test multiple int values into queue buffer and verify order")
    {
        Manager manager{20000, "machine_name"};
        const Tag<int32_t> tag{"id"};

        manager.submit_job("job", [&](Job& job, ManagerHandle) {
            job.declare_publication_intent(tag);
            job.set_buffer(Buffer::Int32Queue());
            job.subscribe(tag).wait();

            for (const auto& value : int_data_set) {
                job.publish(tag, value);
            }

            for (const auto& expected_value : int_data_set) {
                auto pub_fut = job.get_waiter(tag);
                pub_fut.wait();
                std::optional<std::int32_t> maybe_value = pub_fut.get();
                if (maybe_value) {
                    REQUIRE(*maybe_value == expected_value);
                }
            }
        });

        manager.run();
    }

    SECTION("Test multiple double values into queue buffer and verify order")
    {
        Manager manager{20000, "machine_name"};
        const Tag<double> tag{"id"};

        manager.submit_job("job", [&](Job& job, ManagerHandle) {
            job.declare_publication_intent(tag);
            job.set_buffer(Buffer::DoubleQueue());
            job.subscribe(tag).wait();

            for (const auto& value : double_data_set) {
                job.publish(tag, value);
            }

            for (const auto& expected_value : double_data_set) {
                auto pub_fut = job.get_waiter(tag);
                pub_fut.wait();
                std::optional<double> maybe_value = pub_fut.get();
                if (maybe_value) {
                    REQUIRE(*maybe_value == expected_value);
                }
            }
        });

        manager.run();
    }

    SECTION("Test storing larger data set into queue buffer and verify order")
    {
        Manager manager{20000, "machine_name"};
        const Tag<int> tag{"id"};

        manager.submit_job("job", [&](Job& job, ManagerHandle) {
            job.declare_publication_intent(tag);
            job.set_buffer(Buffer::IntQueue());
            job.subscribe(tag).wait();

            for (const auto& value : large_data_set) {
                job.publish(tag, value);
            }

            for (const auto& expected_value : large_data_set) {
                auto pub_fut = job.get_waiter(tag);
                pub_fut.wait();
                std::optional<int> maybe_value = pub_fut.get();
                if (maybe_value) {
                    REQUIRE(*maybe_value == expected_value);
                }
            }
        });

        manager.run();
    }
}

TEST_CASE("Set buffer with custom classes with tuple data types", "[core]")
{

    std::vector<internal::Buffer> buffers;
    buffers.push_back(internal::MostRecentBuffer<int,double>());
    buffers.push_back(internal::QueueBuffer<int,double>());

    for (const auto& buffer : buffers) {
        SECTION("Test with MostRecentBuffer and QueueBuffer classes")
        {
            Manager manager{20000, "machine_name"};
            const Tag<int, double> tag{"id"};

            manager.submit_job("job", [&](Job& job, ManagerHandle) {
                job.declare_publication_intent(tag);
                job.set_buffer(buffer);
                job.subscribe(tag).wait();
                job.publish(tag, 2, 3.0);

                const auto value = job.get_waiter(tag).get();
                REQUIRE(value);
                REQUIRE(std::get<0>(*value) == 2);
                REQUIRE(std::get<1>(*value) == 3.0);
            });

            manager.run();
        }
    }
}