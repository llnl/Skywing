#include "utils.hpp"

#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;

namespace
{
constexpr int num_machines = 4;

using Int32Tag = Tag<std::int32_t>;

void configure_network(const int index, Manager& manager)
{
    // Fully connect the network to ensure that at any point all machines can
    // have a broadcast reach every other machine
    std::vector<std::tuple<std::string, uint16_t>> address_port_pairs(index);
    for (int i = 0; i < index; ++i)
        address_port_pairs[i] =
            std::make_tuple("127.0.0.1", get_starting_port() + i);
    manager.configure_initial_neighbors(address_port_pairs);
}

void check_network(ManagerHandle manager)
{
    using namespace std::chrono_literals;

    // Give some time to allow all of the servers to start
    std::this_thread::sleep_for(10ms);
    while (manager.number_of_neighbors() != num_machines - 1) {
        std::this_thread::sleep_for(1ms);
    }
}

void machine_task(
    const int index,
    const std::array<int, num_machines>* const disconnect_order_ptr)
{
    const auto& disconnect_order = *disconnect_order_ptr;
    using namespace std::chrono_literals;
    Manager manager{static_cast<std::uint16_t>(get_starting_port() + index),
                    std::to_string(index)};
    configure_network(index, manager);
    const auto publish_num =
        *std::find(disconnect_order.cbegin(), disconnect_order.cend(), index);
    const Int32Tag publish_tag{std::to_string(publish_num)};
    manager.submit_job("Job 0", [&](Job& my_job, ManagerHandle manager) {
        check_network(manager);
        my_job.declare_publication_intent(publish_tag);
        std::vector<std::string> subscribe_to;
        for (int i = 0; i < num_machines; ++i) {
            if (i != publish_num) {
                my_job.subscribe(Int32Tag{std::to_string(i)}).wait();
            }
            else {
                while (manager.number_of_subscribers(publish_tag)
                       != static_cast<int>(num_machines - 1))
                {
                    std::this_thread::sleep_for(10ms);
                }
            }
        }
        SKYWING_SYNCHRONIZE_MACHINES(num_machines);
        static std::atomic<int> ready_count{0};
        if (ready_count.fetch_add(1) != static_cast<int>(num_machines - 1)) {
            while (ready_count != static_cast<int>(num_machines)) {
                std::this_thread::sleep_for(10ms);
            }
        }
        my_job.publish(publish_tag, index);
        for (std::size_t i = 0; i < disconnect_order.size(); ++i) {
            const auto to_remove = disconnect_order[i];
            if (to_remove == index) {
                // Leaving the loop will cause the manager to destruct,
                // automatically disconnecting
                break;
            }
            else {
                const Int32Tag get_tag{std::to_string(disconnect_order[i])};
                static std::mutex m;
                std::lock_guard g{m};
                REQUIRE(my_job.get_waiter(get_tag).get() == to_remove);
            }
        }
    });
    manager.run();
    // // Make sure the threads don't exit too soon
    // std::this_thread::sleep_for(1000ms);
}
} // namespace

TEST_CASE("Disconnecting machines doesn't break commuincations.", "[core]")
{
    // Make a random order to disconnect in
    std::array<int, num_machines> disconnect_order;
    std::iota(disconnect_order.begin(), disconnect_order.end(), 0);
    std::shuffle(disconnect_order.begin(), disconnect_order.end(), make_prng());
    std::vector<std::thread> threads;
    for (int i = 0; i < num_machines; ++i) {
        threads.emplace_back(machine_task, i, &disconnect_order);
    }
    for (auto&& thread : threads) {
        thread.join();
    }
}
