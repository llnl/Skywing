#include "utils.hpp"

#include <iostream>
#include <thread>
#include <vector>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/skywing.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;

using namespace std::chrono_literals;

namespace
{

constexpr int size_of_collective = 2;
std::vector<skywing::Tag<int>> tags = {skywing::Tag<int>{"tag0"},
                                       skywing::Tag<int>{"tag1"}};
constexpr std::array<const char*, 2> agent_names{"agent0", "agent1"};
std::vector<size_t> ports{20000, 20001};

void agent_task(const NetworkInfo* const info, const int index)
{
    Manager manager{static_cast<std::uint16_t>(get_starting_port() + index),
                    agent_names[index]};

    configure_network(*info, manager, index, [&](Manager& m, const int i) {
        return m.configure_initial_neighbors("127.0.0.1",
                                             get_starting_port() + i);
    });

    manager.submit_job(
        "pub/sub job", [&](Job& job, ManagerHandle manager_handle) {
            job.declare_publication_intent(tags[index]);
            for (size_t i = index; i < tags.size(); i++)
                job.subscribe(tags[i]).wait();

            int pub_val = 100 * index;
            for (size_t i = 0; i < 10; i++) {
                // Publish a value.
                job.publish(tags[index], pub_val);
                for (size_t i = index; i < tags.size(); i++) {
                    // Request and wait for a new value under the subscription
                    // specified by `tags[i]`.
                    if (std::optional<int> nbr_val =
                            job.get_waiter(tags[i]).get()) {
                        std::cout << agent_names[index] << " received value "
                                  << *nbr_val;
                        std::cout << " from neighbor at port " << ports[i]
                                  << std::endl;
                    }
                }
                pub_val++;

                // Disconnect earlier, after 3 iterations for clean timing
                if (index == 0 && i == 2) {
                    manager_handle.request_disconnect("agent1");
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            std::this_thread::sleep_for(20s);

            std::vector<std::string> neighbor_vec =
                manager.make_neighbor_vector();

            REQUIRE(neighbor_vec.empty());
            REQUIRE(manager.number_of_neighbors() == 0);
        });
    manager.run();
}
} // namespace

TEST_CASE("Simulate agent disconnect", "[core]")
{
    const auto network_info = make_network(size_of_collective, 1);
    std::vector<std::thread> agent_threads;
    for (auto i = 0; i < size_of_collective; ++i) {
        agent_threads.emplace_back(agent_task, &network_info, i);
    }
    for (auto&& thread : agent_threads) {
        thread.join();
    }
}