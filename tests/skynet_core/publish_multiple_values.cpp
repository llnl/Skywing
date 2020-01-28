#include <catch2/catch.hpp>

#include "skynet_core/skynet.hpp"

#include "utils.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>

constexpr int num_machines = 2;
constexpr std::uint16_t base_port = 30000;

using namespace skynet;

using ValueTag = PublishTag<int, double>;
using NotifyTag = PublishTag<>;

constexpr std::tuple<int, double> expected_value{10, 3.14159};
const ValueTag tag0{"tag 0"};
const NotifyTag tag1{"tag 1"};

void machine_task(const NetworkInfo* const info, const int index)
{
  Master master{
    static_cast<std::uint16_t>(base_port + index),
    std::to_string(index)
  };
  connect_network(*info, master, index, [](Master& m, const int i) {
    return m.connect_to_server("127.0.0.1", base_port + i);
  });
  master.submit_job("job", [&](Job& job) {
    if (index == 0)
    {
      job.subscribe(tag1).get();
      // Declare publication intent after subscribing so that the other
      // machine won't publish too early
      job.declare_publication_intent(tag0);
      job.get_future_for(tag1).get();
      job.publish(tag0, expected_value);
      // Wait for message to be sent
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    else
    {
      job.declare_publication_intent(tag1);
      job.subscribe(tag0).get();
      job.publish(tag1);
      const auto val = job.get_future_for(tag0).get();
      REQUIRE(val);
      REQUIRE(*val == expected_value);
    }
  });
  master.run();
}

TEST_CASE("Publishing multiple values works", "[Skynet_MultiplePublish]")
{
  using namespace std::chrono_literals;
  const auto network_info = make_network(num_machines, 1);
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