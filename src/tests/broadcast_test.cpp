#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"
#include "skynet/tag.hpp"

#include <array>
#include <cstdint>
#include <thread>
#include <chrono>

using namespace skynet;

// Emulate multiple machines with multiple threads at this point
// The network looks like this:
//  M1   +--M2
//   |   |   |
//  M3--M4--M5
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

struct SizeTTag : Tag<std::size_t> {};

using JobType = Job<SizeTTag>;

void machine_task(const std::size_t index)
{
  using namespace std::chrono_literals;
  Master master{ports[index], static_cast<std::uint32_t>(index)};
  // Connect to the corresponding machines (if any)
  for (const auto& machine : to_connect[index])
  {
    if (machine == -1)
    {
      break;
    }
    master.connect_to_server("127.0.0.1", ports[machine]);
  }
  // Wait until all machines have connected
  while (master.number_of_neighbors() != machine_counts[index])
  {
    master.accept_pending_connections();
  }
  // Submit job and broadcast on the job using each machine
  auto& my_job = master.create_job<JobType>(0);
  for (std::size_t send_index = 0; send_index < machine_counts.size(); ++send_index)
  {
    if (index == send_index)
    {
      my_job.broadcast<SizeTTag>(send_index);
    }
    else
    {
      while (true)
      {
        master.handle_neighbor_messages();
        if (my_job.has_data<SizeTTag>())
        {
          REQUIRE(my_job.get<SizeTTag>() == send_index);
          break;
        }
      }
    }
  }
}

TEST_CASE("Broadcast works", "[Skynet_Broadcast]")
{
  using namespace std::chrono_literals;
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < machine_counts.size(); ++i)
  {
    threads.emplace_back(machine_task, i);
    std::this_thread::sleep_for(10ms);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
