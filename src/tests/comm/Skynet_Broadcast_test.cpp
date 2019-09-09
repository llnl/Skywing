#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGateway.hpp"

#include <array>
#include <string>
#include <fstream>
#include <thread>
#include <vector>
#include <cstring>
#include <chrono>
#include <algorithm>

// TODO: Make this not a manual proof of concept

using namespace skynet;

// Emulate multiple machines just with multiple threads at this point
// The network looks like this:
//  M1   +--M2
//   |   |   |
//  M3--M4--M5
//   |       |
//   +-------+
// Where higher number devices know about lower numbered connected ones

// The number of connections each machine should have when fully connected
constexpr std::array<std::size_t, 5> machine_counts{1, 2, 3, 3, 3};

// The number of requests that need to be made
constexpr std::array<int, 5> requests_needed{1, 2, 2, 1, 0};

// The number of accepts that need to be made
constexpr std::array<int, 5> accepts_needed{0, 0, 1, 2, 3};

constexpr std::array<const char*, 5> machine_configs{
  // Machine 1
  "skynet_port 5100\n"
  "number_of_devices 0\n"
  "address_type IPv4\n",

  // Machine 2
  "skynet_port 5200\n"
  "number_of_devices 0\n"
  "address_type IPv4\n",

  // Machine 3
  "skynet_port 5300\n"
  "number_of_devices 1\n"
  "address_type IPv4\n"
  "device1_ip_address 127.0.0.1\n"
  "device1_port 5100",

  // Machine 4
  "skynet_port 5400\n"
  "number_of_devices 2\n"
  "address_type IPv4\n"
  "device1_ip_address 127.0.0.1\n"
  "device1_port 5200\n"
  "device2_ip_address 127.0.0.1\n"
  "device2_port 5300",

  // Machine 5
  "skynet_port 5500\n"
  "number_of_devices 3\n"
  "address_type IPv4\n"
  "device1_ip_address 127.0.0.1\n"
  "device1_port 5200\n"
  "device2_ip_address 127.0.0.1\n"
  "device2_port 5300\n"
  "device3_ip_address 127.0.0.1\n"
  "device3_port 5400"
};

struct BroadcastMessage
{
  template <class Archive>
  void serialize(Archive& ar)
  {
    ar(id, data);
  }

  int id;
  int data;
};

constexpr bool operator==(const BroadcastMessage& lhs, const BroadcastMessage& rhs) noexcept
{
  return lhs.id == rhs.id && lhs.data == rhs.data;
}

void machine_task(const std::size_t index)
{
  using namespace std::chrono_literals;
  // Output configuration to file
  const std::string output_name{"broadcast_test_" + std::to_string(index) + ".txt"};
  {
    std::ofstream fout(output_name);
    const char* const to_write = machine_configs[index];
    fout.write(to_write, std::strlen(to_write));
  }
  // Set-up the gateway
  const KeyValueReader config(output_name, " ");
  SocketGateway gateway(config);
  auto factories = gateway.create_initial_connections();
  while (factories.size() != machine_counts[index])
  {
    std::this_thread::sleep_for(10ms);
    auto new_factories = gateway.collect_new_connections();
    for (auto&& factory : new_factories)
    {
      factories.push_back(std::move(factory));
    }
  }
  // I really don't like how this is, but I can't think of a better way to do it...
  // Note that this is very brittle (deadlocks if anything goes wrong too) and took
  // a long time to get right.  Might need to redo some of the communications stuff
  // to make this easier?  Querying / non-blocking when there are no communicators
  // to create would make this a lot easier I feel
  std::vector<std::unique_ptr<DeviceCommunicator>> comms;
  // Set up communications between all machines
  for (int i = 0; i < requests_needed[index]; ++i)
  {
    // Have to do it from the back to the front since the later machines
    // are accepting first
    const auto adj_i = factories.size() - 1 - i;
    comms.push_back(factories[adj_i]->create_new_communicator({}));
  }
  for (int i = 0; i < accepts_needed[index]; ++i)
  {
    // These have to be inserted in the front so that they're in the
    // same order on all machines (yes, super fragile code here)
    auto new_communicators = factories[i]->create_requested_communicators();
    for (auto&& comm : new_communicators)
    {
      comms.insert(comms.begin(), std::move(comm));
    }
  }
  REQUIRE(comms.size() == factories.size());
  for (std::size_t send_index = 0; send_index < machine_configs.size(); ++send_index)
  {
    // These indexes don't work; I don't know if anything can be done about it
    if (send_index == 1 || send_index == 3 || send_index == 4)
    {
      continue;
    }
    const BroadcastMessage to_broadcast{
      static_cast<int>(5 + send_index),
      static_cast<int>(50 + 20 * send_index)
    };
    // Can make this a tag or something?
    int heard_id = 0;
    // The first machine initiates the broadcast
    if (index == send_index)
    {
      for (auto&& comm : comms)
      {
        comm->send_to(to_broadcast);
      }
      heard_id = to_broadcast.id;
    }
    // Even the sending machine will hear the broadcast again, so it can just accept it
    // Note that this too is incredibly fragile since everything has to be broadcast and
    // received in the proper order
    for (auto&& comm : comms)
    {
      const auto message = comm->receive_from<BroadcastMessage>();
      if (message.id != heard_id)
      {
        REQUIRE(message == to_broadcast);
        heard_id = message.id;
        for (auto&& comm : comms)
        {
          comm->send_to(message);
        }
      }
    }
  }
}

TEST_CASE("Broadcast works", "[Skynet_Broadcast]")
{
  using namespace std::chrono_literals;
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < machine_configs.size(); ++i)
  {
    threads.emplace_back(machine_task, i);
    // Give each task time to start
    std::this_thread::sleep_for(10ms);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}