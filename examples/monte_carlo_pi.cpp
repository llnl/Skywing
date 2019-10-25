#include "skynet/skynet.hpp"
#include "utils.hpp"

#include <iomanip>
#include <iostream>
#include <random>
#include <string>

constexpr std::uint16_t base_port = 15000;

using DataTag = skynet::Tag<std::uint64_t>;

void machine_task(
  const skynet::NetworkInfo* const info,
  const int index,
  const int num_machines,
  const int num_iterations
) noexcept
{
  // Init all of the connections
  skynet::Master master{
    static_cast<std::uint16_t>(base_port + index),
    std::to_string(index)
  };
  skynet::connect_network(*info, master, index, [](skynet::Master& m, const int i) {
    return m.connect_to_server("127.0.0.1", base_port + i);
  });
  // Estimate pi / 4 by counting the number of points that land within a quarter
  // of a circle on [0, 1] for both x and y
  master.submit_job(
    std::to_string(index),
    {std::to_string(index)},
    [&](skynet::Job& job) {
      // Subscribe to all of the tags (if machine 0)
      if (index == 0)
      {
        for (auto i = 1; i < num_machines; ++i)
        {
          while (!job.subscribe(DataTag{std::to_string(i)}))
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
          }
        }
      }
      // Count misses rather than hits as they occur less frequently, so less
      // chance of overflow
      std::uint64_t misses = 0;
      auto prng = skynet::make_prng();
      for (auto i = 0; i < num_iterations; ++i)
      {
        std::uniform_real_distribution<double> dist{0, 1};
        const auto x = dist(prng);
        const auto y = dist(prng);
        // If it's outside the circle
        // The circle is centered around (0, 0) so don't need to subtract anything
        // This would normally be
        //   std::sqrt(x * x + y * y) > 1
        // But can square both sides to remove the call to sqrt
        if (x * x + y * y > 1)
        {
          ++misses;
        }
      }
      // TODO: Change this to be a reduce when that's a thing
      // Publish the data if a non-0 machine, otherwise read all the data
      // and display a result
      if (index != 0)
      {
        job.publish(DataTag{std::to_string(index)}, misses);
      }
      else
      {
        for (auto i = 1; i < num_machines; ++i)
        {
          misses += job.get_when_ready(DataTag{std::to_string(i)});
        }
        const double pi_over_4_estimate =
          1.0 -
          misses /
          (static_cast<double>(num_iterations) * num_machines);
        std::cout
          << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
          << "Estimate of pi / 4 is:\n"
          << "1 - " << misses << " / (" << num_iterations << " * " <<  num_machines << ")\n\n"
          << "Which is about:\n"
          << pi_over_4_estimate << "\n\n"
          << "Or, as an estimate of pi:\n"
          << 4 * pi_over_4_estimate << "\n\n";
      }
    }
  );
  master.run();
}

int main(const int argc, const char* const argv[])
{
  using namespace std::chrono_literals;
  if (argc != 3)
  {
    std::cerr
      << "Usage:\n"
      << argv[0] << " num_machines iterations_per_process\n";
    return 1;
  }
  const auto num_machines = std::stoi(argv[1]);
  const auto num_iterations = std::stoi(argv[2]);
  if (num_machines <= 0 || num_iterations <= 0)
  {
    std::cerr << "Invalid number of machines or iterations passed.\n";
    return 1;
  }
  // Create a fully connected network
  const auto network_info = skynet::make_network(
    num_machines,
    skynet::maximum_connections(num_machines)
  );
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(machine_task, &network_info, i, num_machines, num_iterations);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
