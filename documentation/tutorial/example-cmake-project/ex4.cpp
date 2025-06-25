#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <cmath>
#include <list>
#include <random>

#include "skywing_core/manager.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/big_float.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/sum_processor.hpp"
#include "arg_parser.hpp"

using namespace skywing;
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

std::chrono::steady_clock::time_point start_time;

void print_message(size_t agent_num, std::string msg)
{
  auto curr_time = std::chrono::steady_clock::now();
  std::chrono::duration<double> time_diff = curr_time - start_time;
  std::cout << "Time: " << time_diff.count() << "s, Agent " << agent_num;
  std::cout << ", " << msg << std::endl;
}

// used in calculation autoregressive model for battery charge behavior
double binomialCoefficients(int n, int k)
{
  if (k == 0 || k == n)
    return 1.0;
  return binomialCoefficients(n - 1, k - 1) + binomialCoefficients(n - 1, k);
}

/* @brief Model of a battery's charge level over time.
 *
 * The details of how it models charge level over time are not that
 * important to the Skywing example, they are just used to construct a
 * model that has nice behavior.

 * Specifically, it is modeled as an autoregressive (AR) time series
 * model. The characteristic equation of the AR model used here is $(1
 * - decay_rate * B)^order$, where $B$ is the "backshift operator."
 *
 * The `order` parameter specifies the order of the AR model; higher
 * orders creates smoother time series behavior. The `decay_rate`
 * parameter controls how strongly the model's behavior is correlated
 * with past behavior; it must be less than 1.0, and is usually (for
 * our purposes) near 0.99.
 */
class BatteryChargeSensor
{
public:

  /* @brief Construct a BatteryChargeSensor
   *
   * @param order The order of the AR model.
   * @param decay_rate A measure of AR autocorrelation; must be less than 1.0
   */
  BatteryChargeSensor(size_t order, double decay_rate)
    : order_(order), decay_rate_(decay_rate),
      gen_(std::random_device{}()), dist_(0.0, 1.0)
  {
    for (size_t i = 1; i <= order; i++)
      decay_coefficients_.push_back
	(-1 * binomialCoefficients(order, i) * pow(-decay_rate, i));
  }

  /* @brief Set the mean of the noise constributions to the AR model.
   *
   * An unbiased AR model has mean 0.
   *
   * @param mean The new mean.
   */
  void set_sensor_mean(double mean)
  {
    std::lock_guard<std::mutex> lock(mut_);
    dist_ = std::normal_distribution<double>(mean, 1.0);
  }

  /* @brief Draw a new time point from the AR model.
   *
   * @return The new value for the next time point.
   */
  double get_reading()
  {
    // get random noise term
    double new_val;
    {
      std::lock_guard<std::mutex> lock(mut_);
      new_val = dist_(gen_);
    }

    // add decayed prior values
    auto it_coef = decay_coefficients_.begin();
    auto it_vals = prior_values_.begin();
    while (it_coef != decay_coefficients_.end())
    {
      new_val += (*it_vals) * (*it_coef);
      it_coef++;
      it_vals++;
    }

    // if already have `order_` prior values, remove last one
    prior_values_.push_front(new_val);
    if (prior_values_.size() > order_)
      prior_values_.pop_back();

    return new_val;
  }

private:
  size_t order_;
  double decay_rate_;
  std::list<double> prior_values_;
  std::list<double> decay_coefficients_;

  std::mt19937 gen_;
  std::normal_distribution<double> dist_;
  std::mutex mut_;

}; // class BatteryChargeSensor

/* @brief Skywing Job to gather sensor readings and communicate them
   to other Jobs.
 */
void gather_battery_charge_job(Job& job,
			       ManagerHandle manager_handle,
			       size_t agent_number,
			       std::shared_ptr<BatteryChargeSensor> sensor,
			       size_t run_duration)
{
    (void) manager_handle; // required but not needed parameter

    // set up battery sensor publication
    BatteryChargeTag sensor_reading_tag("sensor_reading" + std::to_string(agent_number));
    job.declare_publication_intent(sensor_reading_tag);

    // loop to gather and publish sensor readings
    size_t sensor_frequency_ms = 100;
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(run_duration))
    {
      double sensor_value = sensor->get_reading();
      job.publish(sensor_reading_tag, sensor_value);
      print_message(agent_number, "BatteryChargeSensor: " + std::to_string(sensor_value));
      std::this_thread::sleep_for(std::chrono::milliseconds(sensor_frequency_ms));
    }
}

/* @brief The job that calculates total system charge via collective summation.

 * This job subscribes to battery charge sensor data and then works
 * collectively with the rest of the collective to perform a summation
 * of all battery charge levels.
 *
 * This job then publishes a value that represents its current
 * estimation of the collective total charge sum. This value is
 * continually updated, and it is continually publishing its current
 * estimate.
 *
 * @param job the Job handle associated with this function.
 * @param manager_handle Handle to this agent's Manager.
 * @param agent_number This agent's unique ID number.
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
    BatteryChargeTag sensor_reading_tag("sensor_reading" + std::to_string(agent_number));
    auto waiter = job.subscribe(sensor_reading_tag);
    waiter.wait();

    // set up iterative method:
    // 1. Establish pub/subs for iteration in a circle topology around the collective.
    size_t i = agent_number;
    std::string left_ID = "iter" + std::to_string((i > 0) ? i-1 : size_of_collective-1);
    std::string this_ID = "iter" + std::to_string(i);
    std::string right_ID = "iter" + std::to_string((i < (size_of_collective-1)) ? i+1 : 0);
    std::vector<std::string> iter_sub_tag_IDs{left_ID, this_ID, right_ID};

    // 2. Build and prepare iterative solver.
    double starting_value = 0;
    Waiter<IterMethod> iter_waiter =
        WaiterBuilder<IterMethod>(manager_handle, job, this_ID, iter_sub_tag_IDs)
            .set_processor(starting_value)
            .set_publish_policy()
            .set_iteration_policy(std::chrono::seconds(run_duration))
            .set_resilience_policy()
            .build_waiter();
    IterMethod summation_iteration = iter_waiter.get();

    // Callback called on each iteration of the iterative method.
    auto update_fun = [&](IterMethod& p) {
        // Get the current best guess at the result, print it, and publish it.
        double current_value = p.get_processor().get_value();
	print_message(agent_number, "TotalChargeEstimate: " + std::to_string(current_value));
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
 *
 * If the total charge across all batteries goes over a given
 * threshold, then we take a control action to discharge extra energy
 * from the batteries, gradually lowering charge over time. If the
 * total charge falls below a given threshold, then we take a control
 * action to hold on to more energy, gradually raising charge over
 * time.
 */
void decide_on_actions_job(Job& job,
			   ManagerHandle manager_handle,
			   size_t agent_number,
			   std::shared_ptr<BatteryChargeSensor> sensor,
			   size_t run_duration)
{
    (void) manager_handle; // required but not needed parameter

    // subscribe to state results
    TotalChargeTag total_charge_tag("total_charge");
    job.subscribe(total_charge_tag);

    double high_charge_threshold = 400.0;
    double low_charge_threshold = -400.0;
    double deadband_threshold = 300.0;
    double forced_charge_mag = 0.1;
    size_t decision_frequency_ms = 200;
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(run_duration))
    {
      // get current state estimate
      std::optional<double> current_total_charge = job.get_waiter(total_charge_tag).get();
      if (current_total_charge)
      {
	print_message(agent_number, "ReceivedTotalCharge: " + std::to_string(*current_total_charge));

	// if total charge over threshold, encourage more discharging.
	if (*current_total_charge > high_charge_threshold)
	{
	  sensor->set_sensor_mean(-forced_charge_mag);
	  print_message(agent_number, "Flow: " + std::to_string(-forced_charge_mag));
	}

	// if total charge under threshold, encourage more charging.
	if (*current_total_charge < low_charge_threshold)
	{
	  sensor->set_sensor_mean(forced_charge_mag);
	  print_message(agent_number, "Flow: " + std::to_string(forced_charge_mag));
	}

	// if total charge within normal range, don't encourage either way.
	if (*current_total_charge < deadband_threshold && *current_total_charge > -deadband_threshold)
	{
	  sensor->set_sensor_mean(0.0);
	  print_message(agent_number, "Flow: " + std::to_string(0.0));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(decision_frequency_ms));
      }
    }
}

int main(int argc, char* argv[])
{
    start_time = std::chrono::steady_clock::now();

    // Error checking for the number of arguments
    if (argc < 4) {
        std::cout << "Not Enough Arguments: " << argc << std::endl;
        return 1;
    }

    // collect command line arguments
    ArgParser arg_parser({"agent_number", "starting_port", "size_of_collective",
	"AR_order", "AR_constant", "run_duration"}, argc, argv);

    size_t agent_number = arg_parser.get_arg<size_t>("agent_number");
    std::uint16_t starting_port_number = arg_parser.get_arg<std::uint16_t>("starting_port");
    size_t size_of_collective = arg_parser.get_arg<size_t>("size_of_collective");
    size_t AR_order = arg_parser.get_arg<size_t>("AR_order", 2);
    double AR_constant = arg_parser.get_arg<double>("AR_constant", 0.99);
    size_t run_duration = arg_parser.get_arg<double>("run_duration", 60);

    // given starting port, assign port number to each agent
    std::vector<std::uint16_t> ports;
    for (size_t i = 0; i < size_of_collective; i++)
        ports.push_back(starting_port_number + i);

    // create synthetic BatteryChargeSensor and define sensor reading Job
    //    BatteryChargeSensor sensor(agent_number, sensor_rate);
    std::shared_ptr<BatteryChargeSensor> sensor = std::make_shared<BatteryChargeSensor>(AR_order, AR_constant);

    // define job to gather individual battery charge level
    auto gather_battery_charge_lambda = [agent_number, sensor, run_duration]
      (Job& job, ManagerHandle manager_handle) {
      gather_battery_charge_job(job, manager_handle, agent_number,
				sensor, run_duration);
    };

    // define job to calculate collective total charge level
    auto calculate_total_charge_lambda = [agent_number, size_of_collective, run_duration]
      (Job& job, ManagerHandle manager_handle) {
      calculate_total_charge_job(job, manager_handle, agent_number, size_of_collective,
				 run_duration);
    };

    // define job to make decisions and take actions
    auto decide_on_actions_lambda = [agent_number, sensor, run_duration]
      (Job& job, ManagerHandle manager_handle) {
      decide_on_actions_job(job, manager_handle, agent_number, sensor, run_duration);
    };

    skywing::Manager manager{ports[agent_number], "agent" + std::to_string(agent_number)};
    // Initial agent connection boilerplate.
    if (agent_number != 0)
    {
      manager.configure_initial_neighbors("127.0.0.1", ports[agent_number] - 1);
    }

    manager.submit_job("gather_battery_charge_job", gather_battery_charge_lambda);
    manager.submit_job("calculate_total_charge_job", calculate_total_charge_lambda);
    manager.submit_job("decide_on_action_job", decide_on_actions_lambda);
    manager.run();

    return 0;
}
