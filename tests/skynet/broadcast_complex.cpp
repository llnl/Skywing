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
constexpr int min_connections_per_machine = 1;
constexpr int max_connections_per_machine = 5;
constexpr std::uint16_t base_port = 30000;

using Tag1 = Tag<int>;
using Tag2 = Tag<double>;
using Tag3 = Tag<char>;

using JobType = Job<Tag1, Tag2, Tag3>;

constexpr int tag1_value = 10;
constexpr double tag2_value = 20;
constexpr char tag3_value = 'c';

// For helping with reducing code
template<typename T>
struct ExpectedTagValue;
template<>
struct ExpectedTagValue<Tag1> { static constexpr auto value() { return tag1_value; } };
template<>
struct ExpectedTagValue<Tag2> { static constexpr auto value() { return tag2_value; } };
template<>
struct ExpectedTagValue<Tag3> { static constexpr auto value() { return tag3_value; } };

// If all of the tags are available and the data is correct
// (no C++17 so just make two overloads)
template<typename T1, typename T2, typename Job>
void test_tags(Job& job, const TagID id) noexcept
{
  REQUIRE(job.template get_when_ready(T1(id)) == ExpectedTagValue<T1>::value());
  REQUIRE(job.template get_when_ready(T2(id)) == ExpectedTagValue<T2>::value());
}
template<typename T1, typename T2, typename T3, typename Job>
void test_tags(Job& job, const TagID id) noexcept
{
  REQUIRE(job.template get_when_ready(T1(id)) == ExpectedTagValue<T1>::value());
  REQUIRE(job.template get_when_ready(T2(id)) == ExpectedTagValue<T2>::value());
  REQUIRE(job.template get_when_ready(T3(id)) == ExpectedTagValue<T3>::value());
}

// This wasn't working with a reference, so just use a pointer
void machine_task(Master* const master_ptr, const int index)
{
  using namespace std::chrono_literals;
  auto& master = *master_ptr;
  auto prng = make_prng();
  // Select the maximum number of devices to support
  const int num_connections =
    std::uniform_int_distribution<int>{
      min_connections_per_machine,
      max_connections_per_machine
    }(prng);
  // Select the machines that are going to be connected
  std::vector<int> connections(num_connections);
  std::generate(connections.begin(), connections.end(), [&]() {
    while (true)
    {
      const auto val = std::uniform_int_distribution<int>{0, num_machines - 1}(prng);
      if (val != index)
      {
        return val;
      }
    }
  });
  // Remove any duplicate connections
  std::sort(connections.begin(), connections.end());
  connections.erase(
    std::unique(connections.begin(), connections.end()),
    connections.end()
  );
  // Finally, set up the connections
  // Connect to smaller numbered computers
  for (const auto conn : connections)
  {
    if (conn > index)
    {
      break;
    }
    master.connect_to_server("127.0.0.1", base_port + conn);
  }
  // Forcefully connect to the lower numbered machine to ensure that the graph
  // is fully connected
  const auto lower_machine = base_port + index - 1;
  if (index > 0 && std::find(connections.begin(), connections.end(), lower_machine) == connections.end())
  {
    master.connect_to_server("127.0.0.1", lower_machine);
  }
  // Wait for larger numbered machines to connect
  // There's no way to know when everything is connected, unfortunately, so just
  // wait and assume everyone is connected
  // (Could also pre-generated and parse the connections and do it like that,
  //  probably want to change it to that later)
  const auto end_time = std::chrono::steady_clock::now() + 1s;
  while (std::chrono::steady_clock::now() < end_time)
  {
    master.accept_pending_connections();
    std::this_thread::sleep_for(1ms);
  }
  REQUIRE(master.number_of_neighbors() > 0);
  // Submit first and second job
  auto& job1 = master.make_job<JobType>(0);
  auto& job2 = master.make_job<JobType>(1);
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
      job.global_broadcast(Tag1(job_index), tag1_value);
      test_tags<Tag2, Tag3>(*job_ptr, job_index);
      break;

    case 1:
      job.global_broadcast(Tag2(job_index), tag2_value);
      test_tags<Tag1, Tag3>(*job_ptr, job_index);
      break;

    case 2:
      job.global_broadcast(Tag3(job_index), tag3_value);
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
  // Construct masters here so that they don't disconnect early
  for (auto i = 0; i < num_machines; ++i)
  {
    masters.emplace_back(static_cast<std::uint16_t>(base_port + i), static_cast<std::uint32_t>(i));
  }
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(machine_task, &masters[i], i);
    std::this_thread::sleep_for(10ms);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
