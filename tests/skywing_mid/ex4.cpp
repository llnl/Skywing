#include "utils.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <thread>

#include "skywing_core/manager.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/big_float.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/sum_processor.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace skywing;
using Catch::Matchers::WithinAbs;

namespace ex4
{

using BatteryChargeTag = Tag<double>;
using TotalChargeTag = Tag<double>;
using CountProcessor = QUACCProcessor<BigFloat,
                                      MinProcessor<BigFloat>,
                                      PushFlowProcessor<BigFloat>>;
using SumMethod =
    SumProcessor<double, PushFlowProcessor<double>, CountProcessor>;
using IterMethod = AsynchronousIterative<SumMethod,
                                         AlwaysPublish,
                                         IterateUntilTime,
                                         TrivialResiliencePolicy>;

/* @brief Model of a battery's charge level over time. */
struct BatteryChargeSensor
{
    static constexpr double charge_level_ = 100.0;
};

std::chrono::steady_clock::time_point start_time;

void print_message(size_t agent_num, std::string_view msg)
{
    auto curr_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> time_diff = curr_time - start_time;
    std::cout << "Time: " << time_diff.count() << "s, Agent " << agent_num;
    std::cout << ", " << msg << std::endl;
}

/* @brief Skywing Job to gather sensor readings and communicate them
   to other Jobs.
 */
void gather_battery_charge_job(Job& job,
                               ManagerHandle manager_handle,
                               size_t agent_number,
                               const BatteryChargeSensor& sensor,
                               size_t run_duration)
{
    (void) manager_handle;

    BatteryChargeTag sensor_reading_tag("sensor_reading"
                                        + std::to_string(agent_number));
    job.declare_publication_intent(sensor_reading_tag);

    size_t sensor_frequency_ms = 100;
    while (std::chrono::steady_clock::now() - start_time
           < std::chrono::seconds(run_duration))
    {
        job.publish(sensor_reading_tag, sensor.charge_level_);
        print_message(agent_number,
                      "BatteryChargeSensor: "
                          + std::to_string(sensor.charge_level_));
        std::this_thread::sleep_for(
            std::chrono::milliseconds(sensor_frequency_ms));
    }
}

/* @brief The job that calculates total system charge via collective summation.

 * This job subscribes to battery charge sensor data and then works
 * collectively with the rest of the collective to perform a summation
 * of all battery charge levels.
 */
void calculate_total_charge_job(Job& job,
                                ManagerHandle manager_handle,
                                size_t agent_number,
                                size_t size_of_collective,
                                size_t run_duration)
{
    // set up publishing of state results
    TotalChargeTag total_charge_tag("total_charge");
    job.declare_publication_intent(total_charge_tag);

    // set up subscribing to individual update from other job on this agent
    BatteryChargeTag sensor_reading_tag("sensor_reading"
                                        + std::to_string(agent_number));
    auto waiter = job.subscribe(sensor_reading_tag);
    waiter.wait();

    // set up iterative method:
    // 1. Establish pub/subs for iteration in a circle topology around the
    // collective.
    size_t i = agent_number;
    std::string left_ID =
        "iter" + std::to_string((i > 0) ? i - 1 : size_of_collective - 1);
    std::string this_ID = "iter" + std::to_string(i);
    std::string right_ID =
        "iter" + std::to_string((i < (size_of_collective - 1)) ? i + 1 : 0);
    std::vector<std::string> iter_sub_tag_IDs{left_ID, this_ID, right_ID};

    // 2. Build and prepare iterative solver.
    double starting_value = 0;
    Waiter<IterMethod> iter_waiter =
        WaiterBuilder<IterMethod>(
            manager_handle, job, this_ID, iter_sub_tag_IDs)
            .set_processor(starting_value)
            .set_publish_policy()
            .set_iteration_policy(std::chrono::seconds(run_duration))
            .set_resilience_policy()
            .build_waiter();
    IterMethod summation_iteration = iter_waiter.get();

    // Callback called on each iteration of the iterative method.
    auto update_fun = [&](IterMethod& p) {
        // Get the current best guess at the result
        double current_value = p.get_processor().get_value();
        print_message(agent_number,
                      "TotalChargeEstimate: " + std::to_string(current_value));
        job.publish(total_charge_tag, current_value);

        // Check for an updated sensor reading from the subscription,
        // and incorporate into the iterative method if available.
        std::optional<double> sensor_value =
            job.get_data_if_present(sensor_reading_tag);
        if (sensor_value) {
            p.get_processor().set_value(*sensor_value);
        }
    };

    summation_iteration.run(update_fun);
}

/* @brief Job that collects the result of a state calculating and uses
   it to decide on control actions.
 */
void decide_on_actions_job(Job& job,
                           ManagerHandle manager_handle,
                           size_t agent_number,
                           size_t run_duration)
{
    (void) manager_handle; // required but not needed parameter

    // subscribe to state results
    TotalChargeTag total_charge_tag("total_charge");
    job.subscribe(total_charge_tag);

    size_t decision_frequency_ms = 200;
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start
           < std::chrono::seconds(run_duration))
    {
        // get current state estimate
        std::optional<double> current_total_charge =
            job.get_waiter(total_charge_tag).get();
        if (current_total_charge) {
            print_message(agent_number,
                          "ReceivedTotalCharge: "
                              + std::to_string(*current_total_charge));
            if (std::chrono::steady_clock::now() - start_time
                >= std::chrono::seconds(15))
            {
                REQUIRE_THAT(*current_total_charge,
                             Catch::Matchers::WithinAbs(400., 0.0001));
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(decision_frequency_ms));
        }
    }
}

void agent_task(const NetworkInfo* const info,
                const int agent_number,
                const size_t size_of_collective,
                const double& run_duration,
                const std::vector<std::uint16_t>& ports)
{
    start_time = std::chrono::steady_clock::now();

    // create synthetic BatteryChargeSensor and define sensor reading Job
    constexpr BatteryChargeSensor sensor{};

    // define job to gather individual battery charge level
    auto gather_battery_charge_lambda =
        [agent_number, sensor, run_duration](Job& job,
                                             ManagerHandle manager_handle) {
            gather_battery_charge_job(
                job, manager_handle, agent_number, sensor, run_duration);
        };

    // define job to calculate collective total charge level
    auto calculate_total_charge_lambda =
        [agent_number, size_of_collective, run_duration](
            Job& job, ManagerHandle manager_handle) {
            calculate_total_charge_job(job,
                                       manager_handle,
                                       agent_number,
                                       size_of_collective,
                                       run_duration);
        };

    // define job to make decisions and take actions
    auto decide_on_actions_lambda = [agent_number, run_duration](
                                        Job& job,
                                        ManagerHandle manager_handle) {
        decide_on_actions_job(job, manager_handle, agent_number, run_duration);
    };

    Manager manager{ports[agent_number],
                    "agent" + std::to_string(agent_number)};

    configure_network(
        *info, manager, agent_number, [&](Manager& m, const int agent_number) {
            return m.configure_initial_neighbors("127.0.0.1",
                                                 ports[agent_number]);
        });

    manager.submit_job("gather_battery_charge_job",
                       gather_battery_charge_lambda);
    manager.submit_job("calculate_total_charge_job",
                       calculate_total_charge_lambda);
    manager.submit_job("decide_on_action_job", decide_on_actions_lambda);
    manager.run();
}
} // namespace ex4

TEST_CASE("Example 4", "[mid]")
{
    constexpr size_t size_of_collective = 4;
    constexpr double run_duration = 20;
    const std::uint16_t start_port = get_starting_port();

    // given starting port, assign port number to each agent
    std::vector<std::uint16_t> ports;
    for (size_t i = 0; i < size_of_collective; i++)
        ports.push_back(start_port + i);

    const auto network_info = make_network(size_of_collective, 4);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < size_of_collective; ++i) {
        threads.emplace_back(ex4::agent_task,
                             &network_info,
                             i,
                             size_of_collective,
                             run_duration,
                             ports);
    }
    for (auto&& thread : threads) {
        thread.join();
    }
}