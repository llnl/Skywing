#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"

#include <array>
#include <cstdint>
#include <thread>
#include <chrono>

using namespace skynet;

// Emulate multiple machines with multiple threads at this point
// The network looks like this:
//  M0   +--M1
//   |   |   |
//  M2--M3--M4
//   |       |
//   +-------+
// Higher numbered machines make connection requests to lower numbered ones

// The number of connections each machine should have when fully connected
constexpr std::array<int, 5> machine_counts{1, 2, 3, 3, 3};

// The port each machine is on
std::array<std::uint16_t, 5> ports{
  15000,
  16000,
  17000,
  18000,
  19000
};

// machine connections to make
constexpr std::array<std::array<int, 3>, 5> to_connect{
  std::array<int, 3>{-1, -1, -1},
  std::array<int, 3>{-1, -1, -1},
  std::array<int, 3>{ 0, -1, -1},
  std::array<int, 3>{ 1,  2, -1},
  std::array<int, 3>{ 1,  2,  3}
};

using SizeTTag = Tag<std::size_t>;

using JobType = Job<SizeTTag>;

void setup_network(Master& master, const std::size_t index)
{
  using namespace std::chrono_literals;
  // Connect to the corresponding machines (if any)
  for (const auto& machine : to_connect[index])
  {
    if (machine == -1)
    {
      break;
    }
    if (!master.connect_to_server("127.0.0.1", ports[machine]))
    {
      std::cerr << index << " failed to connect to " << machine << "!\n";
      std::terminate();
    }
  }
  // Wait until all machines have connected
  while (master.number_of_neighbors() != machine_counts[index])
  {
    master.accept_pending_connections();
  }
}

// Reference wasn't working
void machine_task(Master* const master_ptr, const std::size_t index)
{
  auto& master = *master_ptr;
  setup_network(master, index);
  // Submit job and broadcast on the job using each machine
  auto& my_job = master.make_job<JobType>(0);
  // Subscribe to everything ahead of time
  for (std::size_t send_index = 0; send_index < machine_counts.size(); ++send_index)
  {
    my_job.subscribe(SizeTTag(send_index));
  }
  for (std::size_t send_index = 0; send_index < machine_counts.size(); ++send_index)
  {
    if (index == send_index)
    {
      my_job.publish(SizeTTag(send_index), send_index);
    }
    else
    {
      REQUIRE(my_job.get_when_ready(SizeTTag(send_index)) == send_index);
    }
  }
}

TEST_CASE("Broadcast works", "[Skynet_Broadcast]")
{
  using namespace std::chrono_literals;
  // Ensure that the masters live until all threads exit
  std::vector<Master> masters;
  for (std::size_t i = 0; i < machine_counts.size(); ++i)
  {
    masters.emplace_back(ports[i], static_cast<std::uint32_t>(i));
  }
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < machine_counts.size(); ++i)
  {
    threads.emplace_back(machine_task, &masters[i], i);
    std::this_thread::sleep_for(10ms);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
