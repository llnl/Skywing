#include "utils.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>

#include "skywing_core/skywing.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;

namespace
{
constexpr int num_machines = 2;

using ValueTag = Tag<int, double>;
using NotifyTag = Tag<>;

constexpr std::tuple<int, double> publish_value{10, 3.14159};
const ValueTag tag0{"tag 0"};
const NotifyTag tag1{"tag 1"};

void machine_task(const NetworkInfo* const info, const int index)
{
    Manager base_manager{
        static_cast<std::uint16_t>(get_starting_port() + index),
        std::to_string(index)};
    configure_network(*info, base_manager, index, [&](Manager& m, const int i) {
        return m.configure_initial_neighbors("127.0.0.1",
                                             get_starting_port() + i);
    });
    base_manager.submit_job("job", [&](Job& job, ManagerHandle manager) {
        check_network_configuration(*info, manager, index);
        if (index == 0) {
            job.subscribe(tag1).get();
            // Declare publication intent after subscribing so that the other
            // machine won't publish too early
            job.declare_publication_intent(tag0);
            job.get_waiter(tag1).get();
            job.publish(tag0, publish_value);
        }
        else {
            job.declare_publication_intent(tag1);
            job.subscribe(tag0).get();
            job.publish(tag1);
            const auto val = job.get_waiter(tag0).get();
            REQUIRE(val);
            REQUIRE(*val == publish_value);
        }
    });
    base_manager.run();
}
} // namespace

TEST_CASE("Publishing multiple values works", "[core]")
{
    using namespace std::chrono_literals;
    const auto network_info = make_network(num_machines, 1);
    std::vector<std::thread> threads;
    for (auto i = 0; i < num_machines; ++i) {
        threads.emplace_back(machine_task, &network_info, i);
    }
    for (auto&& thread : threads) {
        thread.join();
    }
}
