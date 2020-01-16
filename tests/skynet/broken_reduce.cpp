#include <catch2/catch.hpp>

#include "skynet/skynet.hpp"

#include "utils.hpp"

#include <array>
#include <atomic>
#include <functional>

using namespace skynet;

constexpr int num_machines = 5;
constexpr int num_connections = 1;
constexpr std::uint16_t base_port = 25000;

using ValueTag = ReduceValueTag<std::int32_t>;

const std::array<ValueTag, num_machines> tags{
  ValueTag{"Tag 0"},
  ValueTag{"Tag 1"},
  ValueTag{"Tag 2"},
  ValueTag{"Tag 3"},
  ValueTag{"Tag 4"}
};

const ReduceGroupTag<std::int32_t> reduce_tag{"reduce op"};

std::atomic<int> counter = 0;

// This wasn't working with a reference, so just use a pointer
void machine_task(const NetworkInfo* const info, const int index)
{
  using namespace std::chrono_literals;
  Master master{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
  connect_network(*info, master, index, [](Master& m, const int i) {
    return m.connect_to_server("127.0.0.1", base_port + i);
  });

  master.submit_job("job", [&](Job& the_job) {
    // Create the reduce group
    auto fut = the_job.create_reduce_group(reduce_tag, tags[index], {tags.begin(), tags.end()});
    auto group = fut.get();

    auto reduce1 = group.allreduce(1, std::plus<>{});
    ++counter;
    const auto result1 = reduce1.get();
    REQUIRE_FALSE(result1);
    // group.rebuild();
    std::exit(1);
    const auto result2 = group.allreduce(1, std::plus<>{}).get();
    REQUIRE(result2);
    REQUIRE(*result2 == num_machines);
  });
  master.run();
}

TEST_CASE("Reduce works", "[Skynet_SimpleReduce]")
{
  // spdlog::set_level(spdlog::level::trace);
  const auto network_info = make_network(num_machines, num_connections);
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines - 1; ++i)
  {
    threads.emplace_back(machine_task, &network_info, i);
  }
  for (auto i = 0; i < 2; ++i)
  {
    // Wait a bit to allow machines to process the disconnection
    if (i == 1)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    const auto index = num_machines - 1;
    Master master{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
    connect_network(network_info, master, index, [](Master& m, const int i) {
      return m.connect_to_server("127.0.0.1", base_port + i);
    });
    master.submit_job("job", [&](Job& the_job) {
      auto fut = the_job.create_reduce_group(reduce_tag, tags[index], {tags.begin(), tags.end()});
      auto group = fut.get();

      while (counter != static_cast<int>(num_machines - 1))
      {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
      // Only do the reduce on the second go-around
      if (i == 1)
      {
        auto reduce = group.allreduce(1, std::plus<>{});
        const auto result = reduce.get();
        REQUIRE(result);
        REQUIRE(*result == num_machines);
      }
    });
    master.run();
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}