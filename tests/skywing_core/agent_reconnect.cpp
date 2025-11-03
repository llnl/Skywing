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
constexpr std::array<size_t, 2> ports{20000, 20001};

void agent_task_with_reconnect(const NetworkInfo* const info, const int index)
{
    Manager manager{static_cast<std::uint16_t>(ports[index]),
                    agent_names[index]};

    configure_network(*info, manager, index, [&](Manager& m, const int i) {
        return m.configure_initial_neighbors("127.0.0.1", ports[i]);
    });

    manager.submit_job(
        "pub/sub with reconnect", [&](Job& job, ManagerHandle manager_handle) {
            job.declare_publication_intent(tags[index]);
            for (size_t i = index; i < tags.size(); i++)
                job.subscribe(tags[i]).wait();

            int pub_val = 100 * index;

            // Phase 1: Initial communication
            for (size_t i = 0; i < 3; i++) {
                job.publish(tags[index], pub_val++);
                for (size_t j = index; j < tags.size(); j++) {
                    if (auto val = job.get_waiter(tags[j]).get()) {
                        std::cout << agent_names[index] << " received value "
                                  << *val << " [phase 1]" << std::endl;
                    }
                }
                std::this_thread::sleep_for(1s);
            }

            if (index == 0) {
                manager_handle.request_disconnect("agent1");

                // Wait for OS to fully release the socket
                std::this_thread::sleep_for(15s);

                bool success = manager.request_reconnect("agent1");
                std::cout << "Reconnect returned: " << std::boolalpha << success
                          << std::endl;
                REQUIRE(success);

                std::this_thread::sleep_for(
                    5s); // Allow reconnect to fully settle
            }

            // Agent1 waits until reconnect is visible
            if (index == 1) {
                // wait until agent0 reconnects
                for (int i = 0; i < 100; ++i) {
                    if (manager.number_of_neighbors() == 1)
                        break;
                    std::this_thread::sleep_for(100ms);
                }
                REQUIRE(manager.number_of_neighbors() == 1);

                // Wait for agent0 to finish resubscribing
                std::this_thread::sleep_for(3s);

                // Re-publish values after reconnect
                for (int i = 0; i < 3; ++i) {
                    int repub_val = 200 + i;
                    job.publish(tags[index], repub_val);
                    std::cout << agent_names[index] << " republished value "
                              << repub_val << " [recovery]" << std::endl;
                    std::this_thread::sleep_for(1s);
                }
                std::cout << "agent1 holding to allow reconnect and recovery..."
                          << std::endl;
                std::this_thread::sleep_for(20s); // Adjust as needed
            }

            // Phase 2: Resume pub/sub after reconnect
            for (size_t i = 0; i < 3; i++) {
                job.publish(tags[index], pub_val++);
                for (size_t j = index; j < tags.size(); j++) {
                    if (auto val = job.get_waiter(tags[j]).get()) {
                        std::cout << agent_names[index] << " received value "
                                  << *val << " [phase 2]" << std::endl;
                    }
                }
                std::this_thread::sleep_for(1s);
            }

            // Final validation
            if (index == 0) {
                std::vector<std::string> neighbor_vec =
                    manager.make_neighbor_vector();
                std::cout << "agent0 sees " << neighbor_vec.size()
                          << " neighbors" << std::endl;
                for (const auto& n : neighbor_vec) {
                    std::cout << "agent0 neighbor: " << n << std::endl;
                }

                REQUIRE(neighbor_vec.size() == 1);
                REQUIRE(manager.number_of_neighbors() == 1);
            }
        });

    manager.run();
}

} // namespace

TEST_CASE("Pub/sub with disconnect and reconnect", "[core][reconnect]")
{
    const auto network_info = make_network(size_of_collective, 1);
    std::vector<std::thread> agent_threads;

    for (auto i = 0; i < size_of_collective; ++i) {
        agent_threads.emplace_back(agent_task_with_reconnect, &network_info, i);
    }

    for (auto&& thread : agent_threads) {
        thread.join();
    }
}
