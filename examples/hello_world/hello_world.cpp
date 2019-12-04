#include "skynet/skynet.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

constexpr std::array node_names{
  "node1", "node2", "node3", "node4", "node5"
};

constexpr std::array<std::uint16_t, node_names.size()> node_ports{
  10000, 11000, 12000, 13000, 14000
};

using I32ValueTag = skynet::ReduceValueTag<std::int32_t>;
using I32GroupTag = skynet::ReduceGroupTag<std::int32_t>;

const std::vector<I32ValueTag> reduce_group_tags{
  I32ValueTag{"tag1"},
  I32ValueTag{"tag2"},
  I32ValueTag{"tag3"},
  I32ValueTag{"tag4"},
  I32ValueTag{"tag5"}
};

int main(const int argc, const char* const argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage:\n" << argv[0] << " machine_index\n";
    return 1;
  }
  const int machine_number = [&]() {
    try
    {
      return std::stoi(argv[1]);
    }
    catch (...)
    {
      return -1;
    }
  }();
  if (machine_number < 0 || machine_number >= static_cast<int>(node_ports.size()))
  {
    std::cerr
      << "Invalid machine_index of " << std::quoted(argv[1]) << ".\n"
      << "Must be an integer between 0 and " << node_ports.size() - 1 << '\n';
    return -1;
  }
  skynet::Master master{node_ports[machine_number], node_names[machine_number]};
  if (machine_number != node_ports.size() - 1)
  {
    while (!master.connect_to_server("127.0.0.1", node_ports[machine_number + 1]))
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
  }
  master.submit_job("job", [&](skynet::Job& job) {
    auto reduce_group = job.create_reduce_group(
      I32GroupTag{"random number reduce"},
      reduce_group_tags[machine_number],
      reduce_group_tags
    ).get();
    std::uniform_int_distribution<std::int32_t> random_dist{50, 150};
    std::ranlux48 prng{std::random_device{}()};
    const auto min_value = static_cast<int>(random_dist.min() * node_names.size());
    const auto max_value = static_cast<int>(random_dist.max() * node_names.size());
    std::cout << "Reduce results should be in the range [" << min_value << ", " << max_value << "]\n";
    while (true)
    {
      const auto random_value = random_dist(prng);
      auto [send_ready, value_ready] = reduce_group.allreduce(random_value, std::plus<>{});
      send_ready.get();
      const auto result = value_ready.get();
      const auto cur_time = std::time(nullptr);
      if (result >= min_value && result <= max_value)
      {
        std::cout
          << std::put_time(std::localtime(&cur_time), "[%F %T]")
          << " Allreduce summation: " << result << '\n';
      }
      else
      {
        std::cout
          << std::put_time(std::localtime(&cur_time), "[%F %T]")
          << " !!! Out of range value " << result << " !!!\n";
      }
      // Sleep for a random duration
      std::uniform_int_distribution<int> sleep_dist{10, 1000};
      std::this_thread::sleep_for(std::chrono::milliseconds{sleep_dist(prng)});
    }
  });
  master.run();
}
