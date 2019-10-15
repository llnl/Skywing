#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"

#include "utils.hpp"

#include <chrono>
#include <string>

// TODO: Come up with a better testing scheme, will probably involve
//       actually having multiple (virtual) machines to test so that
//       a machine can be killed without sending out a goodbye message

constexpr int num_machines = 5;
constexpr std::chrono::milliseconds heartbeat_interval{100};
constexpr std::uint16_t base_port = 60000;

using namespace skynet;

void machine_task(Master* const master_ptr, const NetworkInfo* const info, const int index)
{
  auto& master = *master_ptr;
  connect_network(*info, master, index, [](Master& m, const int i) {
    m.connect_to_server("127.0.0.1", base_port + i);
  });
  // Just send heartbeats for a while, give it a dummy job
  Job dummy{"dummy job", master, [](Job&) {
    std::this_thread::sleep_for(heartbeat_interval * 10);
  }};
  master.run();
}

TEST_CASE("Heartbeats are sent", "[Heartbeat_basic]")
{
  using namespace std::chrono_literals;
  std::vector<Master> masters;
  const auto network_info = make_network(num_machines, maximum_connections(num_machines));
  // Construct masters here so that they don't disconnect early
  for (auto i = 0; i < num_machines; ++i)
  {
    masters.emplace_back(
      static_cast<std::uint16_t>(base_port + i),
      std::to_string(i),
      heartbeat_interval
    );
  }
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(machine_task, &masters[i], &network_info, i);
    std::this_thread::sleep_for(10ms);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
