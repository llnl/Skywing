#include "skynet/skynet.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <numeric>
#include <iostream>
#include <iterator>

using namespace skynet;

constexpr int num_machines = 5;
constexpr int num_connections = 1;
constexpr std::uint16_t base_port = 25000;

using ValueTag = ReduceValueTag<std::vector<double>>;

const std::array<ValueTag, num_machines> tags{
  ValueTag{"x0"},
  ValueTag{"x1"},
  ValueTag{"x2"},
  ValueTag{"x3"},
  ValueTag{"x4"}
};

const ReduceGroupTag<std::vector<double>> reduce_tag{"ADMM average x"};

// Solve a linear system with 5 variables, defined below:
// The values were arbitrarily chosen
constexpr std::array<std::array<double, 6>, 5> linear_problems{
  // x1   x2   x3   x4   x5   value
     1,   2,   3,   4,   5,   1114,
    11,  18,   5,  20,  80,  12491,
     8,   1,   4,   1,   2,   3009,
    10,  45,  19,  10,   3,   5816,
     2,   8,  20,  49,  88,  18502
};

// Evaluates an answer and returns the difference from the solution
double target_function(
  const std::array<double, 6>& problem,
  const std::array<double, 5>& solution
)
{
  double sum = 0.0;
  for (std::size_t i = 0; i < solution.size(); ++i)
  {
    sum += problem[i] * solution[i];
  }
  return std::abs(sum - problem.back());
}

double evaluate_solution(
  const std::array<double, 6>& problem,
  const std::array<double, 5>& solution,
  const std::array<double, 5>& global_solution,
  const std::array<double, 5>& y,
  const double roe
)
{
  // f_i(x_i)
  const double f_x = target_function(problem, solution);
  // x - x_i
  const std::array<double, 5> local_minus_global = [&]() {
    std::array<double, 5> ret_val;
    for (std::size_t i = 0; i < 5; ++i)
    {
      ret_val[i] = solution[i] - global_solution[i];
    }
    return ret_val;
  }();
  // y transpose times (x - x_i)
  const double y_scaled = [&]() {
    std::array<double, 5> ret_val;
    for (std::size_t i = 0; i < 5; ++i)
    {
      ret_val[i] = y[i] * local_minus_global[i];
    }
    return std::accumulate(ret_val.cbegin(), ret_val.cend(), 0.0);
  }();
  const double norm_squared = std::accumulate(
    local_minus_global.cbegin(),
    local_minus_global.cend(),
    0.0,
    [](const double so_far, const double next) {
      return so_far + next * next;
    }
  );
  return f_x + y_scaled + roe / 2.0 * norm_squared;
}

// Performs a hill-climbing algorithm to find the minimum of a function
std::array<double, 5> hill_climb(
  const std::array<double, 6>& problem,
  const std::array<double, 5>& initial_guess,
  const std::array<double, 5>& global_solution,
  const std::array<double, 5>& y,
  const double roe,
  const double initial_step_size,
  const double error_threshold
)
{
  auto solution = initial_guess;
  std::array<double, solution.size()> step_sizes;
  std::fill(step_sizes.begin(), step_sizes.end(), initial_step_size);
  int num_iters = 0;
  while (target_function(problem, solution) >= error_threshold && num_iters < 100)
  {
    for (std::size_t i = 0; i < solution.size(); ++i)
    {
      const auto start_distance = evaluate_solution(problem, solution, global_solution, y, roe);
      // Applies a step, keeping it if it improves the value
      const auto apply_step = [&](const double step) {
        solution[i] += step;
        const auto new_distance = evaluate_solution(problem, solution, global_solution, y, roe);
        // No improvement - undo step
        if (new_distance > start_distance)
        {
          solution[i] -= step;
        }
      };
      // Do both plus/minus to try and catch overshooting hills
      const auto start_value = solution[i];
      bool improved = false;
      for (const auto& step : {step_sizes[i], -step_sizes[i]})
      {
        apply_step(step);
        if (start_value != solution[i]) {
          improved = true;
          break;
        }
      }
      // If no improvement was found reduce the step size
      if (!improved)
      {
        step_sizes[i] *= 0.75;
      }
    }
    ++num_iters;
  }
  return solution;
}

void machine_task(const int index)
{
  static std::atomic<int> counter{0};
  using namespace std::chrono_literals;
  Master master{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
  if (index != 0)
  {
    while (!master.connect_to_server("127.0.0.1", base_port + index - 1))
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  master.submit_job("job", [&](Job& the_job) {
    // Create the reduce group
    auto fut = the_job.create_reduce_group(reduce_tag, tags[index], {tags.begin(), tags.end()});
    auto group = fut.get();

    std::array<double, 5> previous_global{INFINITY};
    std::array<double, 5> local_solution{0.0};
    std::array<double, 5> global_solution{0.0};
    std::array<double, 5> y{0.0};
    constexpr double roe = 100;
    constexpr double change_criteria = 0.0000001;

    const auto change_small_enough = [&]() {
      for (std::size_t i = 0; i < 5; ++i)
      {
        if (std::abs(global_solution[i] - previous_global[i]) >= change_criteria)
        {
          return false;
        }
      }
      return true;
    };

    while(!change_small_enough())
    {
      // Update y
      for (std::size_t i = 0; i < 5; ++i)
      {
        y[i] += roe * (local_solution[i] - global_solution[i]);
      }
      // Update the local solution
      local_solution = hill_climb(
        linear_problems[index],
        local_solution,
        global_solution,
        y,
        roe,
        10.0,
        0.00001
      );
      // Update the global solution
      previous_global = global_solution;
      auto fut = group.allreduce(
        {local_solution.cbegin(), local_solution.cend()},
        [&](const std::vector<double>& lhs, const std::vector<double>& rhs) {
          std::vector<double> result{lhs};
          for (std::size_t i = 0; i < lhs.size(); ++i)
          {
            result[i] += rhs[i];
          }
          return result;
        }
      );
      auto new_global = fut.get();
      // Divide by the number of machines (5) now
      for (auto& val : new_global)
      {
        val /= 5.0;
      }
      std::copy(new_global.cbegin(), new_global.cend(), global_solution.begin());

      // Output global x for now
      if (index == 0)
      {
        std::cout << '\t';
        for (std::size_t i = 0; i < 5; ++i)
        {
          std::cout << std::abs(global_solution[i] - previous_global[i]) << ' ';
        }
        std::cout << '\n';
        std::copy(global_solution.cbegin(), global_solution.cend(), std::ostream_iterator<double>{std::cout, " "});
        std::cout << '\n';
      }
    }

    if (index == 0)
    {
      std::cout << "\n------------------------------------\n\n";
      for (const auto& problem : linear_problems)
      {
        std::cout << target_function(problem, global_solution) << ' ';
      }
      std::cout << '\n';
    }

    ++counter;
    while (counter != num_machines)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
  });
  master.run();
}

int main()
{
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(machine_task, i);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
