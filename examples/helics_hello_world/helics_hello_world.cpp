#include "skynet_core/skynet.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

// This is the only include required to use the HELICS API
#include "helics/cpp98/ValueFederate.hpp"

using DataTag = skynet::PublishTag<double>;

// All of the Skynet specific code is located in this function.
void simulate_agent(
  const char* name,
  std::uint16_t local_port,
  std::vector<std::uint16_t>& remote_ports,
  DataTag skynet_publication,
  std::vector<DataTag>& skynet_subscriptions)
{
  // Create a Skynet Master; the Master is responsible for handling communication
  // and other such supporting tasks in the background
  skynet::Master master(local_port, name);
  // Submit work to the master, each job must have a unique name locally, but can be
  // duplicated on other instances.  Jobs run on separate threads than the master and
  // are intended to be where computation and user-defined tasks are done.  Any
  // callable object can be passed as a job, the only restrictions are that it must
  // be copyable and the signature must be compatible with the function signature
  // void(skynet::Job&, skynet::MasterHandle)
  master.submit_job("job", [&](skynet::Job& job, skynet::MasterHandle master_handle) {
    // Connect to other Skynet agents
    for (auto& remote_port : remote_ports) {
      if (remote_port < local_port) continue;
      // Connecting to the server is an asynchronous operation and can fail.
      // Wait for the result each time and keep attempting to connect until it does
      while (!master_handle.connect_to_server("127.0.0.1", remote_port).get()) {
        // Empty
      }
    }

    // Create Skynet publications and subscriptions
    job.declare_publication_intent(skynet_publication);
    auto waiter = job.subscribe_range(skynet_subscriptions);

    // Create the HELICS core/federate combination that this Skynet agent will
    //  use to communicate with the HELICS root broker.  Each Skynet agent
    // will connect to the same root broker using it's own core/federate
    // combination.
    helics_error err = helicsErrorInitialize();
    helics_federate_info fed_info = helicsCreateFederateInfo();
    // tell the core to use zmq sockets to connect to the broker
    helicsFederateInfoSetCoreTypeFromString(fed_info, "zmq", &err);
    if (err.error_code != helics_ok) std::exit(-1);
    // specify that there will only be one federate for this core (there are
    // use cases in the HELICS documentation that detail examples of using
    // multiple federates per core, but the most common use case is a single
    // federate per core... hence the core/federate combination used here)
    helicsFederateInfoSetCoreInitString(fed_info, "--federates=1", &err);
    if (err.error_code != helics_ok) std::exit(-2);
    // name the federate after the local skynet port and create it (it's
    // worth notation that HELICS will assume this federate is at "HELICS time"
    // 0.0 because we did not specify otherwise)
    const char* fed_name = std::to_string(local_port).c_str();
    helics_federate value_fed = helicsCreateValueFederate(fed_name, fed_info, &err);
    if (err.error_code != helics_ok) std::exit(-3);
    // free memory
    helicsFederateInfoFree(fed_info);

    // Register a single HELICS publication of integer data with no units
    // (with the same name as the Skynet publication tag).
    helics_publication helics_pub = helicsFederateRegisterGlobalPublication(
      value_fed, skynet_publication.id().c_str(), helics_data_type_int, NULL, &err);
    if (err.error_code != helics_ok) std::exit(-4);
    // Register a HELICS subscription for each Skynet subscription, using the
    // Skynet subscription tag as a name (and specifying no units).
    std::vector<helics_input> helics_subscriptions;
    helics_subscriptions.reserve(skynet_subscriptions.size());
    for (auto& skynet_subscription : skynet_subscriptions) {
      helics_subscriptions.push_back(
        helicsFederateRegisterSubscription(value_fed, skynet_subscription.id().c_str(), NULL, &err));
      if (err.error_code != helics_ok) std::exit(-5);
    }

    // Transition the HELICS federate to execution mode
    helicsFederateEnterInitializingMode(value_fed, &err);
    if (err.error_code != helics_ok)
      std::cerr << "HELICS failed to enter initialization mode: " << err.message << std::endl;
    helicsFederateEnterExecutingMode(value_fed, &err);
    if (err.error_code != helics_ok) std::cerr << "HELICS failed to enter execution mode: " << err.message << std::endl;

    // Generate random value
    std::uniform_int_distribution random_dist{50, 150};
    std::ranlux48 prng{std::random_device{}()};
    const auto random_value = random_dist(prng);

    // Post publication of random value to HELICS (note this publication will
    //  be at the current "HELICS time" for this agent, which is 0.0)
    helicsPublicationPublishInteger(helics_pub, random_value, &err);
    // Tell HELICS that this Skynet agent would like to advance to "HELICS
    // time" 1.0.
    // This call will block until the Skynet agent hears back from HELICS.
    // HELICS will answer back either when
    // i) data was published to HELICS at a "HELICS time" < 1.0 that this
    //    Skynet agent is subscribed to
    // ii) all federates connected to the HELICS broker have requested to
    //     advance to a "HELICS time" >= 1.0
    // In the case of (i), this Skynet agent will advance to the "HELICS time"
    // when the subscribed data was published.  In the case of (ii), this
    // Skynet agent will advance to "HELICS time" 1.0.  In either case, the
    // current "HELICS time" of this Skynet agent is returned by
    // helicsFederateRequestTime
    helicsFederateRequestTime(value_fed, 1.0, &err);
    if (err.error_code != helics_ok) std::cerr << "HELICS request time failed: " << err.message << std::endl;

    // Retrieve data from HELICS subscriptions
    std::unordered_map<std::string, int> helics_values;
    for (auto& helics_subscription : helics_subscriptions) {
      // Check if no data for this subscription has been published before or
      // at the current "HELICS time" for this Skynet agent
      if (!helicsInputIsUpdated(helics_subscription)) {
        std::cerr << "HELICS value not updated" << std::endl;
        exit(-5);
      }
      const char* key = helicsSubscriptionGetKey(helics_subscription);
      helics_values[key] = helicsInputGetInteger(helics_subscription, &err);
    }

    // Publish random data to Skynet agents
    job.publish(skynet_publication, random_value);

    // Retrieve subscriptions from Skynet agents
    std::unordered_map<std::string, int> skynet_values;
    for (auto& skynet_subscription : skynet_subscriptions) {
      if (job.tag_has_subscription(skynet_subscription))
        skynet_values[skynet_subscription.id()] = *job.get_waiter(skynet_subscription).get();
    }

    // Print all subscription values
    std::cout << std::endl;
    std::cout << "HELICS values" << std::endl;
    for (auto& helics_value : helics_values)
      std::cout << helics_value.first << ": " << helics_value.second << std::endl;
    std::cout << std::endl;
    std::cout << "Skynet values" << std::endl;
    for (auto& skynet_value : skynet_values)
      std::cout << skynet_value.first << ": " << skynet_value.second << std::endl;
    std::cout << std::endl;

    // Shutdown the HELICS core/federate combination
    helicsFederateFinalize(value_fed, &err);
    helicsFederateFree(value_fed);
    helicsCloseLibrary();
  });
  // Start running the master, this will start all submitted jobs and continue
  // running until all jobs are finished, at which point it will return
  master.run();
}

int main(const int argc, const char* const argv[])
{
  // Error checking for the number of arguments
  if (argc < 3) {
    std::cerr << "Usage:\n" << argv[0] << " name local_port group_port1 [group_port2] ...\n";
    return 1;
  }

  // Parse input
  const char* name = argv[1];
  const std::uint16_t local_port = std::stoi(argv[2]);
  std::vector<std::uint16_t> remote_ports;
  remote_ports.reserve(argc - 3);
  for (int i = 3; i < argc; i++)
    remote_ports.emplace_back(std::stoi(argv[i]));

  // Create publication and subscription tags from port numbers
  DataTag publication_tag(std::to_string(local_port));
  std::vector<DataTag> subscription_tags;
  subscription_tags.reserve(argc - 2);
  for (auto& port : remote_ports)
    subscription_tags.emplace_back(std::to_string(port));

  // Simulate the Skynet agent
  simulate_agent(name, local_port, remote_ports, publication_tag, subscription_tags);
}
