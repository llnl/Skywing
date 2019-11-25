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

constexpr int num_machines = 30;
constexpr int num_connections = num_machines * 2;
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
  const auto start = std::chrono::steady_clock::now();
  while (!(job.has_data(tags) && ...))
  {
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(20))
    {
      // std::stringstream ss;
      // ss << id << '-' << job.id() << " has stalled, tag status: ";
      // ((ss << job.has_data(tags)), ...);
      // ss << '\n';
      // std::cerr << ss.str();
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
  // std::stringstream sstr;
  // sstr << index << " ready to go\n";
  // std::cerr << sstr.str();
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
      // This is amazingly ugly, but whatever
      const auto start_time = std::chrono::steady_clock::now();
      std::array<bool, 3> values{false, false, false};
      while(!(values[0] && values[1] && values[2]))
      {
        values = {
          (index == 0 ? true : job.subscribe(std::get<Tag1>(tags[i]))),
          (index == 1 ? true : job.subscribe(std::get<Tag2>(tags[i]))),
          (index == 2 ? true : job.subscribe(std::get<Tag3>(tags[i])))
        };
        if (values[0] && values[1] && values[2]) { break; }
        // std::cerr << std::to_string(index) + " - job" + std::to_string(i) + " still running\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(2))
        {
          std::cerr
            << std::to_string(index) + " job" + std::to_string(i) + " has stalled : "
            + std::to_string(values[0]) + std::to_string(values[1]) + std::to_string(values[2]) + '\n';
          std::this_thread::sleep_for(std::chrono::hours(200));
        }
      }
      // std::stringstream ss; ss << "index " << index << " job " << i << " - start\n"; std::cerr << ss.str();
      if (index < 3)
      {
        // auto last_num = -1;
        while (master.num_subscribers() < num_machines - 1)
        {
          // if (last_num != master.num_subscribers())
          // {
          //   last_num = master.num_subscribers();
          //   std::stringstream sstr;
          //   sstr << "index " << index << " job " << i << " has " << master.num_subscribers() << " subscribers\n";
          //   std::cerr << sstr.str();
          // }
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
      ++ready_counter;
      while (ready_counter != num_machines * 2)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      // Wait for a bit as there's a small chance that a job will publish before the subscription
      // process completes
      // TODO: This doesn't actually seem to be the problem... it is timing relate though, because
      //       it pops up way more after the debug output statements were removed
      //       Seems like subscribe might be returning false positives or something?
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
      // std::stringstream ss2; ss2 << "index " << index << " job " << i << " - end\n"; std::cerr << ss2.str();
    };
  };
  const auto get_tags = [&](const int i) -> std::vector<std::string> {
    const auto& tup = tags[i];
    if      (index == 0) { return {std::get<0>(tup).id()}; }
    else if (index == 1) { return {std::get<1>(tup).id()}; }
    else if (index == 2) { return {std::get<2>(tup).id()}; }
    else                 { return {};                      }
  };
  master.submit_job("job 0", get_tags(0), make_job_task(0));
  master.submit_job("job 1", get_tags(1), make_job_task(1));
  master.run();
}

TEST_CASE("Broadcast works on complex networks", "[Skynet_BroadcastComplex]")
{
  using namespace std::chrono_literals;
  const auto network_info = make_network(num_machines, num_connections);
  // for (std::size_t i = 0; i < network_info.connect_to.size(); ++i)
  // {
  //   std::cerr << i << " -> [";
  //   bool first = true;
  //   for (const auto& conn : network_info.connect_to[i])
  //   {
  //     if (!first)
  //     {
  //       std::cerr << ", ";
  //     }
  //     std::cerr << conn;
  //     first = false;
  //   }
  //   std::cerr << "] {" << network_info.num_connections[i] << "}\n";
  // }
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
