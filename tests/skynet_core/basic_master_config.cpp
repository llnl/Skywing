#include <catch2/catch.hpp>

#include "skynet_core/basic_master_config.hpp"
#include "skynet_core/enable_logging.hpp"
#include "skynet_core/master.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>

constexpr std::array<std::uint16_t, 3> ports{10000, 20000, 30000};

std::mutex catch_mutex;

void machine_task(const int index) noexcept
{
  std::stringstream config_istream;
  config_istream << "machine " << index << '\n' << ports[index] << '\n' << "1000\n";
  for (const auto& conn_to_port : ports) {
    if (conn_to_port != ports[index]) { config_istream << "localhost:" << conn_to_port << '\n'; }
  }
  const auto config = skynet::read_master_config(config_istream);
  {
    std::lock_guard g{catch_mutex};
    REQUIRE(config);
  }
  skynet::Master master{*config};
  std::cerr << index << '\n';
  constexpr std::chrono::seconds time_limit{5};
  const auto start_time = std::chrono::steady_clock::now();
  while (master.number_of_neighbors() != static_cast<int>(ports.size() - 1)) {
    master.accept_pending_connections();
    const auto cur_time = std::chrono::steady_clock::now();
    std::cerr << std::chrono::duration_cast<std::chrono::seconds>(cur_time - start_time).count() << '\n';
    if (cur_time - start_time > time_limit) {
      std::lock_guard g{catch_mutex};
      std::cerr << "Index " << index << " failed to establish connections.\n";
      std::exit(1);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
}

TEST_CASE("Synchronous Iterative", "[Skynet_SynchronousIterative]")
{
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < ports.size(); ++i) {
    threads.emplace_back(machine_task, static_cast<int>(i));
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}
