#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include <skywing_core/skywing.hpp>
#include <skywing_mid/publish_policies.hpp>
#include <skywing_mid/push_flow_processor.hpp>
#include <skywing_mid/iteration_policies.hpp>
#include <skywing_mid/synchronous_iterative.hpp>

std::vector<size_t> ports{20000, 20001, 20002};
std::vector<std::string> tagIDs{"tag0", "tag1", "tag2"};

int main(const int argc, const char* const argv[])
{
    using namespace skywing;
    size_t machine_num = std::stoul(argv[1]);
    std::string machine_name = std::string("machine") + argv[1];
    Manager manager(ports[machine_num], machine_name);
    // make initial connections
    if (machine_num > 0)
        manager.configure_initial_neighbors("localhost",
                                            ports[machine_num - 1]);
    auto pubsub_job = [&](Job& job, ManagerHandle manager_handle) {
        double val_to_contribute = 100.0 * machine_num;

        using MeanMethod = PushFlowProcessor<double, double>;
        using IterMethod = SynchronousIterative<MeanMethod,
                                                IterateUntilTime,
                                                TrivialResiliencePolicy>;
        Waiter<IterMethod> iter_waiter =
            WaiterBuilder<IterMethod>(
                manager_handle, job, tagIDs[machine_num], tagIDs)
                .set_processor(val_to_contribute)
                .set_iteration_policy(std::chrono::seconds(10))
                .set_resilience_policy()
                .build_waiter();

        IterMethod mean_method = iter_waiter.get();

        mean_method.run([&](const IterMethod& p) {
            std::cout << p.run_time().count() << "ms: Machine " << machine_num
                      << " has value " << p.get_processor().get_value()
                      << std::endl;
        });
    };
    manager.submit_job("job", pubsub_job);
    manager.run();
}
