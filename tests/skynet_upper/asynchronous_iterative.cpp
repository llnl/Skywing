#include <catch2/catch.hpp>

#include "skynet_core/enable_logging.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"

#include "utils.hpp"
#include "iterative_test_stuff.hpp"

#include <array>

using namespace skynet;

constexpr int num_machines = 3;
constexpr int num_connections = 1;

const std::array<std::uint16_t, 3> ports{10000, 20000, 30000};

std::vector<ValueTag> tags{ValueTag{"tag0"}, ValueTag{"tag1"}, ValueTag{"tag2"}};

tag_map<std::vector<int>> publish_values
{
  {tags[0], std::vector<int>{0, 10}},
  {tags[1], std::vector<int>{1, 20}},
  {tags[2], std::vector<int>{2, 30}}
};


// int expected_result(ValueTag tag, size_t ind)
// {
//   return publish_values[tag][ind];
// }

std::mutex catch_mutex;


void machine_task(const NetworkInfo* const info, const int index)
{
  Master base_master{ports[index], std::to_string(index)};
  base_master.submit_job("job", [&](Job& job_handle, MasterHandle master) {
    connect_network(*info, master, index, [](MasterHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", ports[i]).get();
    });    

    using IterMethod = AsynchronousIterative<TestAsyncProcessor, TestAsyncPublishPolicy, TestAsyncStopPolicy>;
    IterMethod iter_method = WaiterBuilder<IterMethod>(master, job_handle, tags[index], tags)
      .set_processor(index, publish_values, tags, catch_mutex)
      .set_publish_policy()
      .set_stop_policy(publish_values, tags)
      .set_resilience_policy()
      .build_waiter().get();
    iter_method.run
      (
       []([[maybe_unused]] const IterMethod& c) { std::this_thread::sleep_for(std::chrono::seconds(1));}
      );
    });
  base_master.run();
}

TEST_CASE("Asynchronous Iterative", "[Skynet_AsynchronousIterative]")
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
