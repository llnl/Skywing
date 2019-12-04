#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"

#include "utils.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>

using namespace skynet;

constexpr int num_machines = 20;
constexpr int num_connections = num_machines * 2;
constexpr std::uint16_t base_port = 30000;

using Tag1 = PublishTag<std::int32_t>;
using Tag2 = PublishTag<double>;
using Tag3 = PublishTag<std::string>;

constexpr int tag1_value = 10;
constexpr double tag2_value = 20;
constexpr const char* tag3_value = "test a string";

// For helping with reducing code
template<typename T>
struct ExpectedTagValue;
template<>
struct ExpectedTagValue<Tag1> { static constexpr auto value() { return tag1_value; } };
template<>
struct ExpectedTagValue<Tag2> { static constexpr auto value() { return tag2_value; } };
template<>
struct ExpectedTagValue<Tag3> { static constexpr auto value() { return tag3_value; } };

std::mutex catch_mutex;

// Tests if a specified tag holds the correct value
template<typename Job, typename Tag>
void test_tag(Job& job, const Tag& tag) noexcept
{
  // Catch2's macros are not thread safe
  std::lock_guard g{catch_mutex};
  REQUIRE(job.get_future_for(tag).get() == ExpectedTagValue<Tag>::value());
}
// Tests if the specified tags hold the correct value
template<typename... Tags, typename Job>
void test_tags(Job& job, const Tags&... tags) noexcept
{
  const auto start = std::chrono::steady_clock::now();
  while (!(job.has_data(tags) && ...))
  {
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(20))
    {
      std::cerr << job.id() << " has stalled: " << (std::to_string((int)job.has_data(tags)) + ...) << '\n';
      std::this_thread::sleep_for(std::chrono::hours(200));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  (test_tag(job, tags), ...);
}

// This wasn't working with a reference, so just use a pointer
void machine_task(const NetworkInfo* const info, const int index)
{
  using namespace std::chrono_literals;
  Master master{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
  connect_network(*info, master, index, [](Master& m, const int i) {
    return m.connect_to_server("127.0.0.1", base_port + i);
  });
  // Submit first and second job
  const std::array tags{
    std::make_tuple(Tag1{"job0tag0"}, Tag2{"job0tag1"}, Tag3{"job0tag2"}),
    std::make_tuple(Tag1{"job1tag0"}, Tag2{"job1tag1"}, Tag3{"job1tag2"})
  };
  // For making sure all tasks are ready to go before starting
  static std::atomic<int> ready_counter{0};
  // Function to create a job task
  const auto make_job_task = [&](std::size_t i) {
    return [&tags, &index, &master, i](Job& job) {
      const auto& tag1 = std::get<Tag1>(tags[i]);
      const auto& tag2 = std::get<Tag2>(tags[i]);
      const auto& tag3 = std::get<Tag3>(tags[i]);
      switch (index)
      {
        case 0: job.declare_publication_intent({tag1}); break;
        case 1: job.declare_publication_intent({tag2}); break;
        case 2: job.declare_publication_intent({tag3}); break;
      }
      // subscribe
      switch (index)
      {
      case 0:
        job.subscribe({tag2, tag3}).get();
        break;

      case 1:
        job.subscribe({tag1, tag3}).get();
        break;

      case 2:
        job.subscribe({tag1, tag2}).get();
        break;

      default:
        job.subscribe({tag1, tag2, tag3}).get();
      }
      ++ready_counter;
      while (ready_counter != num_machines * 2)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      switch (index)
      {
      case 0:
        job.publish(tag1, tag1_value);
        test_tags(job, tag2, tag3);
        break;

      case 1:
        job.publish(tag2, tag2_value);
        test_tags(job, tag1, tag3);
        break;

      case 2:
        job.publish(tag3, std::string{tag3_value});
        test_tags(job, tag1, tag2);
        break;

      default:
        test_tags(job, tag1, tag2, tag3);
        break;
      }
    };
  };
  master.submit_job("job 0", make_job_task(0));
  master.submit_job("job 1", make_job_task(1));
  master.run();
}

TEST_CASE("Broadcast works on complex networks", "[Skynet_BroadcastComplex]")
{
  using namespace std::chrono_literals;
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
