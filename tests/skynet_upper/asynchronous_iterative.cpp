#include <catch2/catch.hpp>

#include "skynet_core/enable_logging.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"

#include "utils.hpp"

#include <array>

using namespace skynet;

constexpr int num_machines = 3;
constexpr int num_connections = 1;

using ValueTag = skynet::PublishTag<int>;

const std::array<ValueTag, 3> tags{ValueTag{"tag0"}, ValueTag{"tag1"}, ValueTag{"tag2"}};

const std::array<std::uint16_t, 3> ports{10000, 20000, 30000};

constexpr std::array<std::array<int, 2>, 3> publish_values{
  std::array<int, 2>{0, 10}, std::array<int, 2>{1, 20}, std::array<int, 2>{2, 30}};

std::vector<int> expected_results(const int iter)
{
  assert(iter == 0 || iter == 1);
  return {publish_values[0][iter], publish_values[1][iter], publish_values[2][iter]};
}

std::mutex catch_mutex;

void machine_task(const NetworkInfo* const info, const int index)
{
  Master base_master{ports[index], std::to_string(index)};
  base_master.submit_job("job", [&](Job& job_handle, MasterHandle master) {
    connect_network(*info, master, index, [](MasterHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", ports[i]).get();
    });
    auto opt_iter_method = create_asynchronous_iterative<AsynchronousIterative<int>>
      (master, job_handle, tags[index], tags).get();
    {
      std::lock_guard<std::mutex> g{catch_mutex};
      REQUIRE(opt_iter_method);
    }
    const auto& values_to_publish = publish_values[index];
    auto iter_method = *opt_iter_method;
    for (int i = 0; i < static_cast<int>(values_to_publish.size()); ++i) {
      const auto expected_values = expected_results(i);
      iter_method.submit_values(values_to_publish[i]);
      int values_received = 0;
      while (values_received != static_cast<int>(expected_values.size())) {
        const auto& [values, is_updated, alive_tags] = iter_method.values();
        if (alive_tags.size() != expected_values.size()) {
          std::cerr << "Unexpected connection drop!\n";
          std::exit(1);
        }
        for (int received_index = 0; received_index < static_cast<int>(values.size()); ++received_index) {
          const auto& received_value = values[received_index];
          const auto& updated = is_updated[received_index];
          //          const auto& [received_value, updated] = values[received_index];
          if (updated) {
            std::lock_guard g{catch_mutex};
            ++values_received;
            REQUIRE(received_value == expected_values[received_index]);
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
      }
      SKYNET_SYNCHRONIZE_MACHINES(num_machines);
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
