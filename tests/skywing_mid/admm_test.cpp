#include "utils.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <type_traits>

#include "skywing_core/manager.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_mid/linear_system_processors/admm_processor.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/publish_policies.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;

namespace
{
constexpr int num_machines = 4;
constexpr int num_connections = 1;

using index_t = uint32_t;
using scalar_t = double;

using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;

void machine_task(const NetworkInfo* const info,
                  ClosedMatrix A,
                  ClosedVector b,
                  ClosedVector exact_x,
                  const int index)
{
    const std::uint16_t start_port = get_starting_port();
    const std::array<std::uint16_t, 4> ports{
        start_port,
        static_cast<std::uint16_t>(start_port + 1),
        static_cast<std::uint16_t>(start_port + 2),
        static_cast<std::uint16_t>(start_port + 3)};
    const std::vector<std::string> tag_ids{"tag0", "tag1", "tag2", "tag3"};

    Manager base_manager{ports[index], std::to_string(index)};
    configure_network(*info, base_manager, index, [&](Manager& m, const int i) {
        return m.configure_initial_neighbors("127.0.0.1", ports[i]);
    });
    base_manager.submit_job("job", [&](Job& job_handle, ManagerHandle manager) {
        check_network_configuration(*info, manager, index);

        using IterMethod =
            AsynchronousIterative<ADMMProcessor<index_t, scalar_t>,
                                  AlwaysPublish,
                                  IterateUntilTime,
                                  TrivialResiliencePolicy>;
        IterMethod iter_method =
            WaiterBuilder<IterMethod>(
                manager, job_handle, tag_ids[index], tag_ids)
                .set_processor(A, b)
                .set_publish_policy()
                .set_iteration_policy(std::chrono::seconds(3))
                .set_resilience_policy()
                .build_waiter()
                .get();
        iter_method.run();
        // Compute error comparing to exact solution
        auto soln = iter_method.get_processor().get_value();
        soln -= exact_x;
        REQUIRE(std::sqrt(soln.dot(soln) / exact_x.dot(exact_x)) < 0.01);
    });
    base_manager.run();
}
} // namespace

TEST_CASE("ADMM", "[mid]")
{
    const auto network_info = make_network(num_machines, num_connections);
    std::vector<std::thread> threads;

    // Set-up linear system for each agent (no partition)
    // to minimize via least squares:
    // [1, 2; 4, 5; 7, 8] [ -1; 2] = [3; 6; 9]

    // Define the columns of the matrix
    auto c0 = ClosedVector({{0, 1.0}, {2, 4.0}, {4, 7.0}});
    auto c1 = ClosedVector({{0, 2.0}, {2, 5.0}, {4, 8.0}});
    auto A = ClosedMatrix({{1, c0}, {3, c1}});

    // RHS b - keys correspond to row keys of the matrix
    auto b = ClosedVector({{0, 3.0}, {2, 6.0}, {4, 9.0}});

    // Exact solution x - keys correspond to column keys of the matrix
    auto x = ClosedVector({{1, -1.0}, {3, 2.0}});

    for (auto i = 0; i < num_machines; ++i) {
        threads.emplace_back(machine_task, &network_info, A.transpose(), b, x, i);
    }
    for (auto&& thread : threads) {
        thread.join();
    }
}
