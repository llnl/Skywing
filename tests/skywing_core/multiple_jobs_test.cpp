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
using ValueTag = Tag<int>;

std::vector<ValueTag> tags = {ValueTag{"tag 00"},
                              ValueTag{"tag 01"},
                              ValueTag{"tag 10"},
                              ValueTag{"tag 11"}};

void job_fun(Job& job,
             ManagerHandle manager_handle,
             const NetworkInfo* const info,
             const int agent_index,
             const int job_index)
{
    check_network_configuration(*info, manager_handle, agent_index);
    int job_offset = 2 * job_index;
    int my_index = job_offset + agent_index;
    int other_job_index = job_offset + (1 - agent_index);
    int other_agent_index = (2 - job_offset) + agent_index;

    job.declare_publication_intent(tags[my_index]);

    job.subscribe(tags[other_job_index]).get();
    job.subscribe(tags[other_agent_index]).get();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    int my_val = my_index;
    job.publish(tags[my_index], my_val);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::optional<int> other_job_val =
        job.get_waiter(tags[other_job_index]).get();
    std::optional<int> other_agent_val =
        job.get_waiter(tags[other_agent_index]).get();

    REQUIRE(other_job_val.has_value());
    REQUIRE(other_agent_val.has_value());
    REQUIRE(*other_job_val == other_job_index);
    REQUIRE(*other_agent_val == other_agent_index);
}

void agent_task(const NetworkInfo* const info, const int index)
{
    Manager manager{static_cast<std::uint16_t>(get_starting_port() + index),
                    std::to_string(index)};

    configure_network(*info, manager, index, [&](Manager& m, const int i) {
        return m.configure_initial_neighbors("127.0.0.1",
                                             get_starting_port() + i);
    });
    auto job0 = [&](Job& job, ManagerHandle manager_handle) {
        job_fun(job, manager_handle, info, index, 0);
    };
    auto job1 = [&](Job& job, ManagerHandle manager_handle) {
        job_fun(job, manager_handle, info, index, 1);
    };

    manager.submit_job("job0", job0);
    manager.submit_job("job1", job1);
    manager.run();
}

} // namespace

TEST_CASE("Running multiple jobs in parallel on each agent", "[core]")
{
    using namespace std::chrono_literals;
    const auto network_info = make_network(num_machines, 1);
    std::vector<std::thread> threads;
    for (auto i = 0; i < num_machines; ++i) {
        threads.emplace_back(agent_task, &network_info, i);
    }
    for (auto&& thread : threads) {
        thread.join();
    }
}
