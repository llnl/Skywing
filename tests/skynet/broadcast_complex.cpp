#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"

#include "utils.hpp"

#include <chrono>
#include <cstdint>
#include <random>
#include <thread>
#include <tuple>
#include <vector>

using namespace skynet;

constexpr int num_machines = 50;
constexpr int num_connections = num_machines * 3;
constexpr std::uint16_t base_port = 30000;

using Tag1 = Tag<std::int32_t>;
using Tag2 = Tag<double>;
using Tag3 = Tag<std::string>;

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

// Tests if a specified tag holds the correct value
template<typename Job, typename Tag>
void test_tag(Job& job, const Tag& tag) noexcept
{
  REQUIRE(job.get_when_ready(tag) == ExpectedTagValue<Tag>::value());
}
// Tests if the specified tags hold the correct value
template<typename... Tags, typename Job>
void test_tags(Job& job, const Tags&... tags) noexcept
{
  (test_tag(job, tags), ...);
}
            #include <iostream>
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
    std::make_tuple(Tag1{"job1tag1"}, Tag2{"job1tag2"}, Tag3{"job1tag3"}),
    std::make_tuple(Tag1{"job2tag1"}, Tag2{"job2tag2"}, Tag3{"job2tag3"})
  };
  // Function to create a job task
  const auto make_job_task = [&](std::size_t i) {
    return [&tags, &index, i](Job& job) {
      switch (index)
      {
      case 0:
        job.publish(std::get<Tag1>(tags[i]), tag1_value);
        test_tags(job, std::get<Tag2>(tags[i]), std::get<Tag3>(tags[i]));
        break;

      case 1:
        job.publish(std::get<Tag2>(tags[i]), tag2_value);
        test_tags(job, std::get<Tag1>(tags[i]), std::get<Tag3>(tags[i]));
        break;

      case 2:
        job.publish(std::get<Tag3>(tags[i]), tag3_value);
        test_tags(job, std::get<Tag1>(tags[i]), std::get<Tag2>(tags[i]));
        break;

      default:
        test_tags(job, std::get<Tag1>(tags[i]), std::get<Tag2>(tags[i]), std::get<Tag3>(tags[i]));
        break;
      }
    };
  };
  std::array jobs{
    Job{"job 1", master, make_job_task(0)},
    Job{"job 2", master, make_job_task(1)}
  };
  // Just make sure the tags and jobs are the same size
  static_assert(jobs.size() == tags.size());
  // Subscribe to everything ahead of time
  for (std::size_t i = 0; i < jobs.size(); ++i)
  {
    jobs[i].subscribe(
      std::get<Tag1>(tags[i]),
      std::get<Tag2>(tags[i]),
      std::get<Tag3>(tags[i])
    );
  }
  master.run();
  // Don't exit too early
  std::this_thread::sleep_for(50ms);
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
