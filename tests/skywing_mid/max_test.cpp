#include "utils.hpp"

#include <array>
#include <map>

#include "iterative_test_stuff.hpp"
#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/idempotent_processor.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;
namespace
{
constexpr int num_machines = 4;
constexpr int num_connections = 1;

void machine_task(const NetworkInfo* const info, const int index)
{
    const std::uint16_t start_port = get_starting_port();
    const std::array<std::uint16_t, 4> ports{
        start_port,
        static_cast<std::uint16_t>(start_port + 1),
        static_cast<std::uint16_t>(start_port + 2),
        static_cast<std::uint16_t>(start_port + 3)};
    const std::vector<std::string> tag_ids{"tag0", "tag1", "tag2", "tag3"};

    Manager base_manager{ports[index], std::to_string(index)};
    configure_network(*info, base_manager, index, [&](Manager& m, const int i) {
        return m.configure_initial_neighbors("127.0.0.1", ports[i]);
    });
    base_manager.submit_job("job", [&](Job& job_handle, ManagerHandle manager) {
        check_network_configuration(*info, manager, index);

        using IterMethod = AsynchronousIterative<MaxProcessor<int>,
                                                 AlwaysPublish,
                                                 IterateUntilTime,
                                                 TrivialResiliencePolicy>;
        IterMethod iter_method =
            WaiterBuilder<IterMethod>(
                manager, job_handle, tag_ids[index], tag_ids)
                .set_processor(index)
                .set_publish_policy()
                .set_iteration_policy(std::chrono::seconds(3))
                .set_resilience_policy()
                .build_waiter()
                .get();
        iter_method.run();
        REQUIRE(iter_method.get_processor().get_value() == (num_machines - 1));
    });
    base_manager.run();
}
} // namespace

TEST_CASE("Gossip Max", "[mid]")
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
