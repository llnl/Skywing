#include <catch2/catch.hpp>

#include "skynet_core/master.hpp"
#include "skynet_core/job.hpp"

#include "utils.hpp"

#include <chrono>
#include <string>
#include <iostream>

// TODO: Come up with a better testing scheme, will probably involve
//       actually having multiple (virtual) machines to test so that
//       a machine can be killed without sending out a goodbye message

constexpr int num_machines = 5;
constexpr std::chrono::milliseconds heartbeat_interval{100};
constexpr std::uint16_t base_port = 30000;

using namespace skynet;

void machine_task(const NetworkInfo* const info, const int index)
{
  Master base_master{
    static_cast<std::uint16_t>(base_port + index),
    std::to_string(index),
    heartbeat_interval
  };
  base_master.submit_job("dummy job", [&](Job&, MasterHandle master) {
    connect_network(*info, master, index, [&](MasterHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", base_port + i).get();
    });
    std::this_thread::sleep_for(heartbeat_interval * 10);
  });
  base_master.run();
}

TEST_CASE("Heartbeats are sent", "[Heartbeat_basic]")
{
  using namespace std::chrono_literals;
  const auto network_info = make_network(num_machines, maximum_connections(num_machines));
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(machine_task, &network_info, i);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
