#ifndef SKYNET_TEST_UTILS_HPP
#define SKYNET_TEST_UTILS_HPP

#include "skynet/master.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <random>
#include <vector>

namespace skynet
{
  std::mt19937_64 make_prng() noexcept
  {
    // The number of bytes required for initilizing a Mersenne Twister
    constexpr auto bytes_needed =
        std::mt19937_64::word_size * std::mt19937_64::state_size;
    // Create the initial state
    using result_type = std::random_device::result_type;
    constexpr auto array_size = bytes_needed / sizeof(result_type);
    std::array<result_type, array_size> values;
    std::generate(values.begin(), values.end(), []() {
        return std::random_device{}();
    });
    // Seed the PRNG with the values
    std::seed_seq seq(values.begin(), values.end());
    return std::mt19937_64{seq};
  }

  // A structure representing information to construct a randomly generated network
  struct NetworkInfo
  {
    explicit NetworkInfo(const int num_machines)
      : connect_to(num_machines)
      , num_connections(num_machines)
    {}

    // The machines that each index should connect to
    std::vector<std::vector<int>> connect_to;
    // The number of connections each machine should have when fully connected
    std::vector<int> num_connections;
  };

  // Create a random network with the specified number of machines and roughly
  // the number of connections.  The number of connections will generally exceed
  // the given amount as a random path  is done at the end to make the network
  // fully connected
  NetworkInfo make_network(const int num_machines, const int num_connections)
  {
    assert(num_machines > 1);
    // The total number of connections possible is the sum from 1 to
    // (number of machines - 1) which is equal to n * (n + 1) / 2
    const auto maximum_connections = [](int n) { return (n - 1) * n / 2; };
    assert(num_connections < maximum_connections(num_connections));
    // Reserve enough room in the return object for each machine
    NetworkInfo to_ret{num_machines};
    // Adds a random connection; doing nothing if it already exists or is a
    // self-edge
    const auto add_edge = [&](const int a, const int b) {
      if (a == b)
      {
        return;
      }
      const auto [low, high] = std::minmax(a, b);
      // Lower numbered machines connect to higher ones
      auto& add_to = to_ret.connect_to[low];
      // Don't add an edge that already exists
      if (std::find(add_to.cbegin(), add_to.cend(), high) != add_to.cend())
      {
        return;
      }
      add_to.push_back(high);
      ++to_ret.num_connections[low];
      ++to_ret.num_connections[high];
    };
    auto prng = make_prng();
    // Add random edges up to the limit
    for (int i = 0; i < num_connections; ++i)
    {
      std::uniform_int_distribution<int> dist{0, num_machines - 1};
      add_edge(dist(prng), dist(prng));
    }
    // Add a random path that goes through all of the nodes
    std::vector<int> random_path(num_machines);
    std::iota(random_path.begin(), random_path.end(), 0);
    std::shuffle(random_path.begin(), random_path.end(), prng);
    for (std::size_t i = 0; i < random_path.size() - 1; ++i)
    {
      add_edge(random_path[i], random_path[i + 1]);
    }
    // Finally, sort the connections
    for (auto& conns : to_ret.connect_to)
    {
      std::sort(conns.begin(), conns.end());
    }
    return to_ret;
  }

  // Performs the required steps to create the network from a NetworkInfo
  // The connection argument should have the signature `void(Master&, int)`
  // with the int parameter corresponding to the index of the machine to
  // connect to
  template<typename Callable>
  void connect_network(const NetworkInfo& info, Master& master, const int index, Callable connect)
  {
    using namespace std::chrono_literals;
    for (const auto connect_to : info.connect_to[index])
    {
      connect(master, connect_to);
    }
    while (master.number_of_neighbors() != info.num_connections[index])
    {
      master.accept_pending_connections();
      std::this_thread::sleep_for(1ms);
    }
  }
} // namespace skynet

#endif // SKYNET_TEST_UTILS_HPP
