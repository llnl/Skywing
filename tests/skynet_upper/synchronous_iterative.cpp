#include <catch2/catch.hpp>

#include "skynet_core/enable_logging.hpp"

#include "skynet_core/enable_logging.hpp"
#include "skynet_upper/synchronous_iterative.hpp"

#include "utils.hpp"

#include <array>
#include <map>

using namespace skynet;

constexpr int num_machines = 4;
constexpr int num_connections = 1;

const std::uint16_t start_port = get_starting_port();

using ValueTag = skynet::PublishTag<int>;
using PrivateValueTag = skynet::PrivateTag<int>;

const std::array<ValueTag, 4> tags{ValueTag{"tag0"}, ValueTag{"tag1"}, ValueTag{"tag2"}, ValueTag{"tag3"}};
const std::array<PrivateValueTag, 2> private_tags{PrivateValueTag{"tag0"}, PrivateValueTag{"tag1"}};

const std::array<std::uint16_t, 4> ports{start_port, static_cast<std::uint16_t>(start_port + 1), static_cast<std::uint16_t>(start_port + 2), static_cast<std::uint16_t>(start_port + 3)};

constexpr std::array<std::array<int, 2>, 4> publish_values{
  std::array<int, 2>{0, 10}, std::array<int, 2>{1, 20}, std::array<int, 2>{2, 30}, std::array<int, 2>{3, 40}};

std::vector<int> expected_results(const int iter)
{
  assert(iter == 0 || iter == 1);
  return {publish_values[0][iter], publish_values[1][iter], publish_values[2][iter], publish_values[3][iter]};
}

std::vector<int> expected_results_private(const int iter)
{
  assert(iter == 0 || iter == 1);
  return {publish_values[0][iter], publish_values[1][iter]};
}

const std::map<PrivateValueTag, std::vector<std::string>> nodes{
  {private_tags[0], {"localhost:" + std::to_string(ports[0]), "localhost:" + std::to_string(ports[1])}},
  {private_tags[1], {"localhost:" + std::to_string(ports[2]), "localhost:" + std::to_string(ports[3])}}
};

std::mutex catch_mutex;

void machine_task(const NetworkInfo* const info, const int index)
{
  Master base_master{ports[index], std::to_string(index)};
  base_master.submit_job("job", [&](Job& job_handle, MasterHandle master) {
    connect_network(*info, master, index, [](MasterHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", ports[i]).get();
    });
    ///////////////////////////////
    // Normal iterative method
    ///////////////////////////////
    auto opt_iter_method = create_synchronous_iterative(master, job_handle, tags[index], tags).get();
    {
      std::lock_guard<std::mutex> g{catch_mutex};
      REQUIRE(opt_iter_method);
    }
    const auto& values_to_publish = publish_values[index];
    auto iter_method = *opt_iter_method;
    for (int i = 0; i < static_cast<int>(values_to_publish.size()); ++i) {
      const auto values = iter_method.values(values_to_publish[i]).get();
      {
        std::lock_guard g{catch_mutex};
        REQUIRE(values == expected_results(i));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    ///////////////////////////////////
    // Supernode iterative method
    ///////////////////////////////////
    auto super_opt_iter_method = create_supernode_synchronous_iterative(master, job_handle, private_tags[index / 2], nodes).get();
    {
      std::lock_guard<std::mutex> g{catch_mutex};
      REQUIRE(super_opt_iter_method);
    }
    auto super_iter = *super_opt_iter_method;
    const auto& private_values_to_publish = publish_values[index / 2];
    for (int i = 0; i < static_cast<int>(private_values_to_publish.size()); ++i) {
      const auto values = super_iter.values(private_values_to_publish[i]).get();
      {
        std::lock_guard g{catch_mutex};
        REQUIRE(values == expected_results_private(i));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    // Test erroring
    if (index == 0) {
      const auto values = super_iter.values(private_values_to_publish[0] + 1).get();
      REQUIRE(values.empty());
    }
    else {
      const auto values = super_iter.values(private_values_to_publish[0]).get();
      {
        std::lock_guard g{catch_mutex};
        REQUIRE(values.empty());
      }
    }
  });
  base_master.run();
}

TEST_CASE("Synchronous Iterative", "[Skynet_SynchronousIterative]")
{
  const auto network_info = make_network(num_machines, num_connections);
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i) {
    threads.emplace_back(machine_task, &network_info, i);
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}
