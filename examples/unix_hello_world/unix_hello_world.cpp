#include <unistd.h>

#include <chrono>
#include <iostream>
#include <vector>

// #include "skywing_core/enable_logging.hpp"
#include "skywing_core/skywing.hpp"

// This example shows a simple program of agents sending information
// to each other through publication and subscription streams. The
// collective in this example has 3 agents. Each agent will produce
// its own publication stream to which the other agents subscribe.

// Socket names used by each agent in the collective.
std::vector<std::string> addrs = {"sock0", "sock1", "sock2"};

// Tags, which are collective-unique identifiers of publication
// streams, are statically types with the data being sent through the
// publication stream. In this case, we will be sending ints through
// the publication streams.
using pubtag_t = skywing::Tag<int>;

// The tags associated with each publication stream. Agent 0 (using
// socket 'sock0') will publish under tag "tag0", Agent 1 (using
// socket 'sock1') will publish under tag "tag1", and Agent 2 (using
// socket 'sock2') will publish under tag "tag2".
std::vector<pubtag_t> tags = {
    pubtag_t{"tag0"}, pubtag_t{"tag1"}, pubtag_t{"tag2"}};

int main(const int argc, const char* const argv[])
{
    using namespace skywing;

    if (argc < 2) {
        std::cerr << "Usage: Not Enough Arguments: " << argc << std::endl;
        return EXIT_FAILURE;
    }

    // Parse the agent number that was passed in to distinguish which
    // agent this process is.
    size_t const agent_num = std::stoul(argv[1]);
    if (agent_num > addrs.size()) {
        std::cerr << "Agent number must be less than " << addrs.size()
                  << std::endl;
        return EXIT_FAILURE;
    }

    // Establish agent name based on its number.
    std::string const agent_name = std::string("agent") + argv[1];

    // Create a Skywing Manager; the Manager is responsible for handling
    // communication between agents.
    Manager manager(addrs[agent_num], agent_name);

    // Configure initial handshake connections between neighboring
    // agents (Note: the actual connections are made later).
    // Every agent does NOT need to be neighbors (or even
    // aware of) all agents in the collective. The requirement is only
    // that the set of connections, when viewed as edges in an
    // undirected graph, forms a connected graph.
    // In this call, Agent 2 will reach out to Agent 1, and Agent 1
    // will reach out to Agent 0. Agent 0 will not directly reach out
    // to any agent, but this is fine as the graph will still be
    // connected. Note that in a true deployment, the need for
    // resilience would demand additional connections so that the
    // failure of a single Agent can't disconnect the graph.
    if (agent_num > 0) {
        manager.configure_initial_neighbors(addrs[agent_num - 1]);
    }

    // Create the function that will be passed to the Skywing manager as
    // a Job. Everything inside this lambda function will execute in its
    // own thread once we call `manager.run()`.
    auto pubsub_job = [&](Job& job, ManagerHandle) {
        // Each agent declares that it intends to publish a stream under a given
        // tag.
        job.declare_publication_intent(tags[agent_num]);

        // Each agent subscribes to its own publication as well as those
        // of higher-indexed agents.
        for (size_t i = agent_num; i < tags.size(); i++)
            job.subscribe(tags[i]).wait();

        // Repeatedly submit and receive values.
        int pub_val = 100 * agent_num;
        for (size_t i = 0; i < 10; i++) {
            // Publish a value.
            job.publish(tags[agent_num], pub_val);
            for (size_t i = agent_num; i < tags.size(); i++) {
                // Request for a new value under the subscription
                // specified by `tags[i]`.
                if (std::optional<int> nbr_val =
                        job.get_data_if_present(tags[i]);
                    nbr_val.has_value())
                {
                    std::cout << agent_name << " received value " << *nbr_val
                              << " from neighbor \"" << addrs[i] << "\""
                              << std::endl;
                }
            }
            // Increase the value published by this agent and wait a second.
            pub_val++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    };

    manager.submit_job("job", pubsub_job);
    manager.run();
}
