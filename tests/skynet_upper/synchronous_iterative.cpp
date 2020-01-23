#include <catch2/catch.hpp>

#include "skynet_upper/synchronous_iterative.hpp"

#include "utils.hpp"

#include <array>

using namespace skynet;

constexpr int num_machines = 3;
constexpr int num_connections = 1;

using ValueTag = skynet::PublishTag<int>;

const std::array<ValueTag, 3> tags{
  ValueTag{"tag1"}, ValueTag{"tag2"}, ValueTag{"tag3"}
};

const std::array<std::uint16_t, 3> ports{
  10000, 20000, 30000
};

constexpr std::array<std::array<int, 2>, 3> publish_values{
  std::array<int, 2>{0, 10},
  std::array<int, 2>{1, 20},
  std::array<int, 2>{2, 30}
};

constexpr std::tuple<int, int> expected_results(const int index, const int iter)
{
  assert(iter == 0 || iter == 1);
  if      (index == 0) { return {publish_values[1][iter], publish_values[2][iter]}; }
  else if (index == 1) { return {publish_values[0][iter], publish_values[2][iter]}; }
  else if (index == 2) { return {publish_values[0][iter], publish_values[1][iter]}; }
  else                 { assert(false); return {0, 0}; }
}

std::mutex catch_mutex;

void machine_task(const NetworkInfo* const info, const int index)
{
  Master master{ports[index], std::to_string(index)};
  connect_network(*info, master, index, [](Master& m, const int i) {
    return m.connect_to_server("127.0.0.1", ports[i]);
  });
  master.submit_job("job", [&](Job& job_handle) {
    const auto& to_publish = publish_values[index];
    const SynchronousIterative iter_method = [&]() {
      if (index == 0)
      {
        return SynchronousIterative{job_handle, tags[0], to_publish[0], tags[1], tags[2]};
      }
      else if (index == 1)
      {
        return SynchronousIterative{job_handle, tags[1], to_publish[0], tags[0], tags[2]};
      }
      else
      {
        return SynchronousIterative{job_handle, tags[2], to_publish[0], tags[0], tags[1]};
      }
    }();
    // Test that the initial values are okay
    {
      std::lock_guard g{catch_mutex};
      REQUIRE(iter_method.values() == expected_results(index, 0));
    }
    // Submit next values and then test second array
    iter_method.submit_value(to_publish[1]);
    {
      std::lock_guard g{catch_mutex};
      REQUIRE(iter_method.values() == expected_results(index, 1));
    }
  });
  master.run();
}

TEST_CASE("Synchronous Iterative", "[Skynet_SynchronousIterative]")
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
