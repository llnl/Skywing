#include <catch2/catch.hpp>

#include "skynet/master.hpp"
#include "skynet/job.hpp"

#include "utils.hpp"

#include <cstdint>
#include <thread>
#include <chrono>
#include <random>
#include <vector>

using namespace skynet;

constexpr int num_machines = 50;
constexpr int num_connections = num_machines * 3;
constexpr std::uint16_t base_port = 30000;

using Tag1 = Tag<int>;
using Tag2 = Tag<double>;
using Tag3 = Tag<long>;

using JobType = Job<Tag1, Tag2, Tag3>;

constexpr int tag1_value = 10;
constexpr double tag2_value = 20;
constexpr long tag3_value = 1023570;

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
void test_tag(Job& job, const Tag tag) noexcept
{
  REQUIRE(job.get_when_ready(tag) == ExpectedTagValue<Tag>::value());
}
// Tests if the specified tags hold the correct value
template<typename... Tags, typename Job>
void test_tags(Job& job, const TagID id) noexcept
{
  (test_tag(job, Tags(id)), ...);
}

// This wasn't working with a reference, so just use a pointer
void machine_task(Master* const master_ptr, const NetworkInfo* const info, const int index)
{
  using namespace std::chrono_literals;
  auto& master = *master_ptr;
  connect_network(*info, master, index, [](Master& m, const int i) {
    m.connect_to_server("127.0.0.1", base_port + i);
  });
  // Submit first and second job
  auto& job1 = master.make_job<JobType>(0);
  auto& job2 = master.make_job<JobType>(1);
  // Subscribe to everything ahead of time
  job1.subscribe(Tag1{0}, Tag2{0}, Tag3{0});
  job2.subscribe(Tag1{1}, Tag2{1}, Tag3{1});
  // First three machines do broadcasts on different tags for both jobs
  // Pointer in intitilizer list since things in them are always const
  // The first machines also then have to propagate the other broadcasts
  for (auto job_ptr : {&job1, &job2})
  {
    auto& job = *job_ptr;
    const auto job_index = (job_ptr == &job1 ? 0 : 1);
    switch (index)
    {
    case 0:
      job.publish(Tag1(job_index), tag1_value);
      test_tags<Tag2, Tag3>(*job_ptr, job_index);
      break;

    case 1:
      job.publish(Tag2(job_index), tag2_value);
      test_tags<Tag1, Tag3>(*job_ptr, job_index);
      break;

    case 2:
      job.publish(Tag3(job_index), tag3_value);
      test_tags<Tag1, Tag2>(*job_ptr, job_index);
      break;

    default:
      test_tags<Tag1, Tag2, Tag3>(*job_ptr, job_index);
      break;
    }
  }
}

TEST_CASE("Broadcast works on complex networks", "[Skynet_BroadcastComplex]")
{
  using namespace std::chrono_literals;
  std::vector<Master> masters;
  const auto network_info = make_network(num_machines, num_connections);
  // Construct masters here so that they don't disconnect early
  for (auto i = 0; i < num_machines; ++i)
  {
    masters.emplace_back(static_cast<std::uint16_t>(base_port + i), static_cast<std::uint32_t>(i));
  }
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(machine_task, &masters[i], &network_info, i);
    std::this_thread::sleep_for(10ms);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
