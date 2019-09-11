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
    if (auto factory = gateway.collect_new_connection())
    {
      factories.push_back(std::move(factory));
    }
  }
  std::vector<std::unique_ptr<DeviceCommunicator>> comms;
  // Set up communications between all machines
  for (int i = 0; i < requests_needed[index]; ++i)
  {
    // Have to do it from the back to the front since the later machines
    // are accepting first
    const auto adj_i = factories.size() - 1 - i;
    comms.push_back(factories[adj_i]->create_new_communicator({}));
  }
  while (comms.size() != factories.size())
  {
    for (auto&& factory : factories)
    {
      if (auto new_comm = factory->create_requested_communicator())
      {
        comms.push_back(std::move(new_comm));
      }
    }
  }
  // Future: Can tag data and then have a buffer for if stuff arrives out of order
  int last_heard = 0;
  for (std::size_t send_index = 0; send_index < machine_configs.size(); ++send_index)
  {
    const BroadcastMessage to_broadcast{
      static_cast<int>(5 + send_index),
      static_cast<int>(50 + 20 * send_index)
    };
    // The first machine initiates the broadcast
    if (index == send_index)
    {
      for (auto&& comm : comms)
      {
        comm->send(to_broadcast);
      }
      last_heard = to_broadcast.id;
    }
    else
    {
      bool done = false;
      while (!done)
      {
        for (auto&& comm : comms)
        {
          // listen for messages from all neighbors
          const auto message_pair = comm->receive<BroadcastMessage>();
          // Make sure an actual message was heard
          if (!message_pair.first)
          {
            continue;
          }
          const auto& message = message_pair.second;
          // got a message, ignore it if it's old
          if (message.id <= last_heard)
          {
            continue;
          }
          // Otherwise make sure it's the same and broadcast it to all neighbors aside from the sender
          REQUIRE(message == to_broadcast);
          last_heard = message.id;
          for (auto&& neighbor : comms)
          {
            if (std::addressof(neighbor) != std::addressof(comm))
            {
              neighbor->send(message);
            }
          }
          done = true;
          break;
        }
        // Make it not a busy loop
        std::this_thread::sleep_for(10us);
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
