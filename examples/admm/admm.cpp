#include "skynet/skynet.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>

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
constexpr std::array<std::array<double, num_machines + 1>, num_machines> linear_problems{
  // x0   x1   x2   x3   x4   value
     1,   2,   3,   4,   5,   1114,
    11,  18,   5,  20,  80,  12491,
     8,   1,   4,   1,   2,   3009,
    10,  45,  19,  10,   3,   5816,
     2,   8,  20,  49,  88,  18502
};
constexpr std::array<double, num_machines> real_solution{
  247244569.0 / 219675.0,
  74157917.0 / 219675.0,
  -458561492.0 / 219675.0,
  420005392.0 / 219675.0,
  -32145303.0 / 73225.0
};

// Evaluates an answer and returns the difference from the solution
double target_function(
  const std::array<double, num_machines + 1>& problem,
  const std::array<double, num_machines>& solution
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
  const std::array<double, num_machines + 1>& problem,
  const std::array<double, num_machines>& solution,
  const std::array<double, num_machines>& global_solution,
  const std::array<double, num_machines>& y,
  const double roe
)
{
  // f_i(x_i)
  const double f_x = target_function(problem, solution);
  // x - x_i
  const std::array<double, num_machines> local_minus_global = [&]() {
    std::array<double, num_machines> ret_val;
    for (std::size_t i = 0; i < ret_val.size(); ++i)
    {
      ret_val[i] = solution[i] - global_solution[i];
    }
    return ret_val;
  }();
  // y transpose times (x - x_i)
  const double y_scaled = [&]() {
    std::array<double, num_machines> ret_val;
    for (std::size_t i = 0; i < ret_val.size(); ++i)
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
  return f_x + y_scaled + (roe / 2.0) * norm_squared;
}

// Performs a hill-climbing algorithm to find the minimum of a function
std::array<double, num_machines> hill_climb(
  const std::array<double, num_machines + 1>& problem,
  const std::array<double, num_machines>& initial_guess,
  const std::array<double, num_machines>& global_solution,
  const std::array<double, num_machines>& y,
  const double roe,
  const double initial_step_size,
  const double error_threshold
)
{
  auto solution = initial_guess;
  std::array<double, solution.size()> step_sizes;
  std::fill(step_sizes.begin(), step_sizes.end(), initial_step_size);
  int num_iters = 0;
  while (evaluate_solution(problem, solution, global_solution, y, roe) >= error_threshold && num_iters < 100)
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
        if (start_value != solution[i])
        {
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
    constexpr double min_starting = -10.0;
    constexpr double max_starting = 10.0;

    // Randomly initialize the local solution
    std::array<double, num_machines> local_solution = [&]() {
      std::array<double, num_machines> to_ret;
      auto prng = std::ranlux48{std::random_device{}()};
      std::generate(
        to_ret.begin(),
        to_ret.end(),
        [&]() mutable {
          return std::uniform_real_distribution{min_starting, max_starting}(prng);
      });
      return to_ret;
    }();
    std::array<double, num_machines> global_solution{0.0};
    std::array<double, num_machines> y{0.0};
    constexpr double roe = 5.0;
    constexpr double convergence_criteria = .0001;

    // Output statistics for the estimated result versus the actual
    constexpr int output_width = 11;
    constexpr int full_row_width = 11 * 2 + 3;
    const auto output_status = [&](int iter_num) {
      std::cout
        << std::setfill('-') << std::setw(full_row_width) << '-' << '\n'
        << std::setfill(' ') << "Iter " << std::setw(full_row_width - 5) << iter_num << '\n'
        << std::setfill('-') << std::setw(full_row_width) << '-' << '\n'
        << std::setfill(' ')
        << std::setw(output_width) << "Actual" << " | " << std::setw(output_width) << "Estimated" << '\n'
        << std::setfill('-') << std::setw(output_width) << '-'
          << "-+-" << std::setw(output_width) << '-' << std::setfill(' ') << '\n'
        << std::setprecision(3) << std::fixed;
      for (std::size_t i = 0; i < real_solution.size(); ++i)
      {
        std::cout << std::setw(output_width) << real_solution[i]
          << " | " << std::setw(output_width) << global_solution[i] << '\n';
      }
      std::cout << std::setfill('-') << std::setw(full_row_width) << '-' << '\n';
    };

    int iter_num = 0;
    for (; true; ++iter_num)
    {
      if (index == 0 && iter_num != 0 && iter_num % 5'000 == 0)
      {
        output_status(iter_num);
      }

      const auto change_small_enough = [&]() {
        for (std::size_t i = 0; i < global_solution.size(); ++i)
        {
          if (std::abs(global_solution[i] - local_solution[i]) >= convergence_criteria)
          {
            return false;
          }
        }
        return true;
      };
      // First value is to indicate convergence
      std::vector<double> to_send(num_machines + 1);
      // TODO: Allow sending different typed values so stuff like this can be avoided in the future
      //   This will require quite a lot of work / thinking about how to support it, though
      std::copy(local_solution.cbegin(), local_solution.cend(), to_send.begin() + 1);
      to_send.front() = change_small_enough() ? 1.0 : -1.0;
      // Update the global solution
      auto fut = group.allreduce(
        to_send,
        [&](const std::vector<double>& lhs, const std::vector<double>& rhs) {
          std::vector<double> result{lhs};
          for (std::size_t i = 1; i < lhs.size(); ++i)
          {
            result[i] += rhs[i];
          }
          // Not yet converged; lhs is already checked because result copies from lhs
          if (rhs.front() < 0.0)
          {
            result.front() = -1.0;
          }
          return result;
        }
      );
      auto new_global = fut.get();
      // Divide by the number of machines for the average value
      for (auto& val : new_global)
      {
        val /= num_machines;
      }
      std::copy(new_global.cbegin() + 1, new_global.cend(), global_solution.begin());
      // This has converged if this is the case
      if (new_global.front() > 0.0)
      {
        break;
      }

      // Update y
      for (std::size_t i = 0; i < local_solution.size(); ++i)
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
        100.0,
        0.00001
      );
    }

    if (index == 0)
    {
      std::cout
        << "\n\n"
        << "----------------\n"
        << "- FINAL RESULT -\n"
        << "----------------\n\n";
      output_status(iter_num + 1);
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
