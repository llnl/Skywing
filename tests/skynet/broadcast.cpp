#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

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

// The names of the machines
constexpr std::array<const char*, 5> machine_names{"m0", "m1", "m2", "m3", "m4"};

// The names of the tags
constexpr std::array<const char*, 5> tag_names{"t0", "t1", "t2", "t3", "t4"};

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

using Uint64Tag = Tag<std::uint64_t>;

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
    while (!master.connect_to_server("127.0.0.1", ports[machine]))
    {
      std::this_thread::sleep_for(10ms);
    }
  }
  // Wait until all machines have connected
  while (master.number_of_neighbors() != machine_counts[index])
  {
    master.accept_pending_connections();
    std::this_thread::sleep_for(10ms);
  }
}

// Reference wasn't working
void machine_task(const std::size_t index)
{
  Master master{ports[index], machine_names[index]};
  setup_network(master, index);
  // Submit job and broadcast on the job using each machine
  master.submit_job("job 0", {tag_names[index]}, [&master, index](Job& the_job) {
    // Subscribe to everything ahead of time
    for (std::size_t send_index = 0; send_index < machine_counts.size(); ++send_index)
    {
      if (index != send_index)
      {
        while (!the_job.subscribe(Uint64Tag{tag_names[send_index]}))
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
    }
    while (master.num_subscribers() != machine_counts.size() - 1)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (std::size_t send_index = 0; send_index < machine_counts.size(); ++send_index)
    {
      if (index == send_index)
      {
        the_job.publish(Uint64Tag{tag_names[index]}, index);
      }
      else
      {
        REQUIRE(the_job.get_when_ready(Uint64Tag{tag_names[send_index]}) == send_index);
      }
    }
  });
  // Start processing messages
  master.run();
}

TEST_CASE("Broadcast works", "[Skynet_Broadcast]")
{
  using namespace std::chrono_literals;
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < machine_counts.size(); ++i)
  {
    threads.emplace_back(machine_task, i);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
