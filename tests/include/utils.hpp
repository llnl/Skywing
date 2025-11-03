#ifndef SKYWING_TEST_UTILS_HPP
#define SKYWING_TEST_UTILS_HPP

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "skywing_core/internal/buffer.hpp"
#include "skywing_core/manager.hpp"
#include <catch2/catch_test_macros.hpp>

namespace skywing
{
inline std::mt19937_64 make_prng() noexcept
{
    // The number of bytes required for initilizing a Mersenne Twister
    constexpr auto bytes_needed =
        std::mt19937_64::word_size * std::mt19937_64::state_size;
    // Create the initial state
    using result_type = std::random_device::result_type;
    constexpr auto array_size = bytes_needed / sizeof(result_type);
    std::array<result_type, array_size> values;
    std::generate(
        values.begin(), values.end(), []() { return std::random_device{}(); });
    // Seed the PRNG with the values
    std::seed_seq seq(values.begin(), values.end());
    return std::mt19937_64{seq};
}

// Reads the starting port number from the environment, exiting the program on
// failure
inline std::uint16_t get_starting_port()
{
    std::uint16_t port = 0;
    if (port == 0) {
        char const* const port_str = std::getenv("START_PORT");
        if (!port_str) {
            throw std::runtime_error(
                "Could not find environment variable START_PORT");
        }
        auto const candidate = std::atol(port_str);
        if (candidate == 0) {
            std::ostringstream msg;
            msg << "Error parsing START_PORT (value is \"" << port_str << "\")";
            throw std::runtime_error(msg.str());
        }
        if (candidate > 0xFFFF || candidate < 0) {
            std::ostringstream msg;
            msg << "START_PORT value is too high (value is \"" << port_str
                << "\")";
            throw std::runtime_error(msg.str());
        }
        port = static_cast<std::uint16_t>(candidate);
    }
    return port;
}

// Creates a container of the specified type for ports to connect to
inline std::vector<std::uint16_t> create_ports(std::size_t num)
{
    const auto start_port = get_starting_port();
    std::vector<std::uint16_t> ports(num);
    std::iota(ports.begin(), ports.end(), start_port);
    return ports;
}

// A structure representing information to construct a randomly generated
// network
struct NetworkInfo
{
    explicit NetworkInfo(const int num_machines)
        : connect_to(num_machines), num_connections(num_machines)
    {}

    friend std::ostream& operator<<(std::ostream& out, const NetworkInfo& info)
    {
        int i = 0;
        for (const auto& conn : info.connect_to) {
            out << i << " -> [";
            bool first = true;
            for (const auto& machine : conn) {
                if (!first) {
                    out << ", ";
                }
                out << machine;
                first = false;
            }
            out << "]\n";
            ++i;
        }
        return out;
    }

    // The machines that each index should connect to
    std::vector<std::vector<int>> connect_to;
    // The number of connections each machine should have when fully connected
    std::vector<int> num_connections;
};

// Returns the maximum number of connections possible for a given number of
// machines
inline constexpr int maximum_connections(const int num_machines)
{
    // The total number of connections possible is the sum from 1 to
    // (number of machines - 1) which is equal to n * (n + 1) / 2
    // (Where n = number of machines - 1)
    return (num_machines - 1) * num_machines / 2;
}

// Create a random network with the specified number of machines and roughly
// the number of connections.  The number of connections will generally exceed
// the given amount as a random path is done at the end to make sure there are
// no "islands" in the graph
inline NetworkInfo make_network(const int num_machines,
                                const int num_connections)
{
    assert(num_machines > 1);
    assert(num_connections <= maximum_connections(num_machines));
    // Reserve enough room in the return object for each machine
    NetworkInfo to_ret{num_machines};
    // Adds a random connection; doing nothing if it already exists or is a
    // self-edge
    const auto add_edge = [&](const int a, const int b) {
        if (a == b) {
            return;
        }
        const auto [low, high] = std::minmax(a, b);
        // Lower numbered machines connect to higher ones
        auto& add_to = to_ret.connect_to[low];
        // Don't add an edge that already exists
        if (std::find(add_to.cbegin(), add_to.cend(), high) != add_to.cend()) {
            return;
        }
        add_to.push_back(high);
        ++to_ret.num_connections[low];
        ++to_ret.num_connections[high];
    };
    auto prng = make_prng();
    // Add random edges up to the limit
    for (int i = 0; i < num_connections; ++i) {
        std::uniform_int_distribution<int> dist{0, num_machines - 1};
        add_edge(dist(prng), dist(prng));
    }
    // Add a random path that goes through all of the nodes
    std::vector<int> random_path(num_machines);
    std::iota(random_path.begin(), random_path.end(), 0);
    std::shuffle(random_path.begin(), random_path.end(), prng);
    for (std::size_t i = 0; i < random_path.size() - 1; ++i) {
        add_edge(random_path[i], random_path[i + 1]);
    }
    // Finally, sort the connections
    for (auto& conns : to_ret.connect_to) {
        std::sort(conns.begin(), conns.end());
    }
    return to_ret;
}

// Performs the required steps to configure the network from a NetworkInfo
// The connection argument should have the signature `void(ManagerHandle, int)`
// with the int parameter corresponding to the index of the machine to
// connect to, and blocking until connected (or timeout)
template <typename Callable>
void configure_network(const NetworkInfo& info,
                       Manager& manager,
                       const int index,
                       Callable connect)
{
    for (const auto connect_to : info.connect_to[index]) {
        connect(manager, connect_to);
    }
}

// This checks that the initial requested connections have been made.
inline void check_network_configuration(const NetworkInfo& info,
                                        ManagerHandle& manager,
                                        const int index)
{
    using namespace std::chrono_literals;
    while (manager.number_of_neighbors() != info.num_connections[index]) {
        std::this_thread::sleep_for(1ms);
    }
}

template <typename... Ts>
class SubscribeDataAssert
{
public:
    SubscribeDataAssert() = delete;
    SubscribeDataAssert(std::span<PublishValueVariant> data)
        : data_{internal::detail::make_value<Ts...>(
            data, std::index_sequence_for<Ts...>{})}
    {}

    void isStoredUnderTag(const Tag<Ts...>& tag, Job& job)
    {
        auto waiter = job.get_waiter(tag);
        waiter.wait_for(wait_time);
        const auto stored_data = waiter.get();
        CHECK(stored_data);
        REQUIRE(data_ == stored_data);
    }

private:
    ValueOrTuple<Ts...> data_;
    std::chrono::milliseconds wait_time{1000};
};

} // namespace skywing

// Macro to synchronize all machines
#define SKYWING_SYNCHRONIZE_MACHINES(machine_count)                            \
    []() noexcept {                                                            \
        static std::size_t sync{0};                                            \
        static std::mutex m;                                                   \
        static std::condition_variable cv;                                     \
        std::unique_lock lock{m};                                              \
        ++sync;                                                                \
        if (sync % static_cast<std::size_t>(machine_count) != 0) {             \
            cv.wait(lock, [&]() {                                              \
                return sync % static_cast<std::size_t>(machine_count) == 0;    \
            });                                                                \
        }                                                                      \
        else {                                                                 \
            cv.notify_all();                                                   \
        }                                                                      \
    }()

#endif // SKYWING_TEST_UTILS_HPP
