#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"

#include "utils.hpp"

#include <vector>
#include <thread>
#include <random>
#include <numeric>

using namespace skynet;

static constexpr std::uint16_t base_port = 5000;
static constexpr int num_machines = 10;

using IntTag = Tag<int>;
using JobType = Job<IntTag>;

void setup_network(const int index, Master& master)
{
  using namespace std::chrono_literals;
  // Give some time to allow all of the servers to start
  std::this_thread::sleep_for(10ms);
  // Fully connect the network to ensure that at any point all machines can have a
  // broadcast reach every other machine
  for (int i = 0; i < index; ++i)
  {
    master.connect_to_server("127.0.0.1", base_port + i);
  }
  while (master.number_of_neighbors() != num_machines - 1)
  {
    master.accept_pending_connections();
    std::this_thread::sleep_for(1ms);
  }
}

void machine_task(const int index, const std::array<int, num_machines>* const disconnect_order_ptr)
{
  const auto& disconnect_order = *disconnect_order_ptr;
  using namespace std::chrono_literals;
  Master master{static_cast<std::uint16_t>(base_port + index), static_cast<MachineID>(index)};
  setup_network(index, master);
  auto& my_job = master.make_job<JobType>(0);
  // Subscribe to all tags ahead of time
  for (std::size_t i = 0; i < disconnect_order.size(); ++i)
  {
    my_job.subscribe(IntTag(i));
  }
  for (std::size_t i = 0; i < disconnect_order.size(); ++i)
  {
    const auto to_remove = disconnect_order[i];
    if (to_remove == index)
    {
      // broadcast and remove (the data doesn't really matter)
      my_job.publish(IntTag(i), to_remove);
      // Leaving the loop will cause the master to destruct, automatically
      // disconnecting
      break;
    }
    else
    {
      REQUIRE(my_job.get_when_ready(IntTag(i)) == to_remove);
    }
  }
  // Make sure the threads don't exit too soon
  std::this_thread::sleep_for(1000ms);
}

TEST_CASE("Disconnecting machines don't break commuincations.", "[Skynet_Disconnect]")
{
  // Make a random order to disconnect in
  std::array<int, num_machines> disconnect_order;
  std::iota(disconnect_order.begin(), disconnect_order.end(), 0);
  std::shuffle(disconnect_order.begin(), disconnect_order.end(), make_prng());
  std::vector<std::thread> threads;
  for (int i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(machine_task, i, &disconnect_order);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
