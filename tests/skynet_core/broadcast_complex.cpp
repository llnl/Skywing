#include <catch2/catch.hpp>

#include "skynet_core/master.hpp"
#include "skynet_core/job.hpp"
#include "skynet_core/enable_logging.hpp"

#include "utils.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <tuple>
#include <vector>

using namespace skynet;

constexpr int num_machines = 10;
constexpr int num_connections = num_machines * 2;
constexpr std::uint16_t base_port = 30000;

using Tag0 = PublishTag<std::int32_t>;
using Tag1 = PublishTag<double>;
using Tag2 = PublishTag<std::string>;

constexpr int tag1_value = 10;
constexpr double tag2_value = 20;
constexpr const char* tag3_value = "test a string";

// For helping with reducing code
template<typename T>
struct ExpectedTagValue;
template<>
struct ExpectedTagValue<Tag0> { static constexpr auto value() { return tag1_value; } };
template<>
struct ExpectedTagValue<Tag1> { static constexpr auto value() { return tag2_value; } };
template<>
struct ExpectedTagValue<Tag2> { static constexpr auto value() { return tag3_value; } };

std::mutex catch_mutex;

const std::array<std::tuple<Tag0, Tag1, Tag2>, 2> tags{
  std::make_tuple(Tag0{"job0tag0"}, Tag1{"job0tag1"}, Tag2{"job0tag2"}),
  std::make_tuple(Tag0{"job1tag0"}, Tag1{"job1tag1"}, Tag2{"job1tag2"})
};

template<std::size_t TagIndex, std::size_t ValueIndex>
void test_tag(
  Job& job,
  MasterHandle master_handle,
  const int machine_index,
  const int job_index
) noexcept
{
  using TagType = std::tuple_element_t<TagIndex, decltype(tags)::value_type>;
  constexpr auto value_to_publish = ExpectedTagValue<TagType>::value();
  const auto& tag = std::get<TagIndex>(tags[ValueIndex]);
  if (machine_index == TagIndex && job_index == ValueIndex)
  {
    // As there are two jobs, each machine will end up subscribing to itself
    while (master_handle.number_of_subscribers(tag) != num_machines)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    job.publish(tag, value_to_publish);
  }
  else
  {
    job.subscribe(tag).get();
    while (true)
    {
      const auto opt_value = job.get_waiter(tag).get();
      if (opt_value)
      {
        // Catch2's macros are not thread safe
        std::lock_guard g{catch_mutex};
        REQUIRE(*opt_value == value_to_publish);
        break;
      }
      else
      {
        job.rebuild_missing_tag_connections().get();
      }
    }
  }
  SKYNET_SYNCHRONIZE_MACHINES(num_machines * 2);
}

// This wasn't working with a reference, so just use a pointer
void machine_task(const NetworkInfo* const info, const int index)
{
  using namespace std::chrono_literals;
  Master master{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
  // Function to create a job task
  const auto make_job_task = [&](std::size_t i) {
    return [index, info, i](Job& job, MasterHandle handle) {
      if (i == 0)
      {
        connect_network(*info, handle, index, [](MasterHandle& m, const int num) {
          return m.connect_to_server("127.0.0.1", base_port + num).get();
        });
      }
      SKYNET_SYNCHRONIZE_MACHINES(num_machines * 2);
      switch (index)
      {
        case 0: job.declare_publication_intent(std::get<Tag0>(tags[i])); break;
        case 1: job.declare_publication_intent(std::get<Tag1>(tags[i])); break;
        case 2: job.declare_publication_intent(std::get<Tag2>(tags[i])); break;
      }
      test_tag<0, 0>(job, handle, index, i);
      test_tag<0, 1>(job, handle, index, i);
      test_tag<1, 0>(job, handle, index, i);
      test_tag<1, 1>(job, handle, index, i);
      test_tag<2, 0>(job, handle, index, i);
      test_tag<2, 1>(job, handle, index, i);
    };
  };
  master.submit_job("job 0", make_job_task(0));
  master.submit_job("job 1", make_job_task(1));
  master.run();
}

TEST_CASE("Broadcast works on complex networks", "[Skynet_BroadcastComplex]")
{
  SKYNET_SET_LOG_LEVEL_TO_TRACE();
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
