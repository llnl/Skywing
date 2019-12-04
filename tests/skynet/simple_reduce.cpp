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

template<typename Group, typename Callable>
void test_reduce(Group& group, const std::int32_t value, Callable reduce_op, const std::int32_t expected_value)
{
  static std::mutex catch_mutex;
  // Normal reduce
  const auto first_result = group.reduce(value, reduce_op).get();
  // Allreduce
  auto allreduce_futures = group.allreduce(value, reduce_op);
  // Wait for the value to be ready / propagated
  allreduce_futures.first.get();
  const auto second_result = allreduce_futures.second.get();
  std::lock_guard g{catch_mutex};
  if (group.returns_value_on_reduce())
  {
    REQUIRE(first_result);
    REQUIRE(*first_result == expected_value);
  }
  else
  {
    REQUIRE_FALSE(first_result);
  }
  REQUIRE(second_result == expected_value);
}

// This wasn't working with a reference, so just use a pointer
void machine_task(const NetworkInfo* const info, const int index)
{
  static std::atomic<int> counter{0};
  using namespace std::chrono_literals;
  Master master{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
  connect_network(*info, master, index, [](Master& m, const int i) {
    return m.connect_to_server("127.0.0.1", base_port + i);
  });
  master.submit_job("job", [&](Job& the_job) {
    // Create the reduce group
    auto fut = the_job.create_reduce_group(reduce_tag, tags[index], {tags.begin(), tags.end()});
    auto group = fut.get();

    // Do a few reduce operations on the group
    using i32 = std::int32_t;
    test_reduce(group, index, std::plus<>{}, 10);
    test_reduce(group, index, [](i32 a, i32 b) { return std::max(a, b); }, 4);
    test_reduce(group, index, [](i32 a, i32 b) { return std::min(a, b); }, 0);
    test_reduce(group, index + 1, std::multiplies<>{}, 1 * 2 * 3 * 4 * 5);

    ++counter;
    while (counter != num_machines)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
  });
  master.run();
}

TEST_CASE("Reduce works", "[Skynet_SimpleReduce]")
{
  const auto network_info = make_network(num_machines, num_connections);
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
