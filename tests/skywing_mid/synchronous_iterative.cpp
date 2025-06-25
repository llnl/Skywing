#include "skywing_mid/synchronous_iterative.hpp"

#include "utils.hpp"

#include <array>
#include <map>

#include "iterative_test_stuff.hpp"
#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;

namespace
{
constexpr int num_machines = 4;
constexpr int num_connections = 1;

std::vector<std::string> tag_ids{"tag0", "tag1", "tag2", "tag3"};

std::unordered_map<std::size_t, std::vector<int>> publish_values{
    {0, std::vector<int>{0, 10}},
    {1, std::vector<int>{1, 20}},
    {2, std::vector<int>{2, 30}},
    {3, std::vector<int>{3, 40}}};

void machine_task(const NetworkInfo* const info, const int index)
{
    auto const start_port = get_starting_port();
    const std::array<std::uint16_t, 4> ports{
        start_port,
        static_cast<std::uint16_t>(start_port + 1),
        static_cast<std::uint16_t>(start_port + 2),
        static_cast<std::uint16_t>(start_port + 3)};

    Manager base_manager{ports[index], std::to_string(index)};
    configure_network(*info, base_manager, index, [&](Manager& m, const int i) {
        return m.configure_initial_neighbors("127.0.0.1", ports[i]);
    });
    base_manager.submit_job("job", [&](Job& job_handle, ManagerHandle manager) {
        check_network_configuration(*info, manager, index);

        ///////////////////////////////
        // Normal iterative method
        ///////////////////////////////
        using IterMethod = SynchronousIterative<TestAsyncProcessor,
                                                IterateUntilTime,
                                                TrivialResiliencePolicy>;
        IterMethod iter_method =
            WaiterBuilder<IterMethod>(
                manager, job_handle, tag_ids[index], tag_ids)
                .set_processor(index, num_machines)
                .set_iteration_policy(std::chrono::seconds(10))
                .set_resilience_policy()
                .build_waiter()
                .get();
        iter_method.run();
        REQUIRE(fabs(iter_method.get_processor().get_curr_average()
                     - iter_method.get_processor().get_target())
                < 0.02);
    });
    base_manager.run();
}
} // namespace

TEST_CASE("Synchronous Iterative", "[mid]")
{
    const auto network_info = make_network(num_machines, num_connections);
    std::vector<std::thread> threads;
    for (auto i = 0; i < num_machines; ++i) {
        threads.emplace_back(machine_task, &network_info, i);
    }
    for (auto&& thread : threads) {
        thread.join();
    }
}
