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
  274244569.0 / 219675.0,
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
  return std::pow(sum - problem.back(), 2);
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
  constexpr int max_iters = 1'000;
  auto solution = initial_guess;
  std::array<double, solution.size()> step_sizes;
  std::fill(step_sizes.begin(), step_sizes.end(), initial_step_size);
  int num_iters = 0;
  while (evaluate_solution(problem, solution, global_solution, y, roe) >= error_threshold && num_iters < max_iters)
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

template<typename GetGlobalAndConverged>
void admm_work(const std::size_t index, GetGlobalAndConverged get_global_and_converged) noexcept
//  requires requires(const std::array<double, 5>& local_solution, bool locally_converged)
//  {
//    { get_global_and_converged(local_solution, locally_converged) }
//      -> std::tuple<std::array<double, num_machines>, bool>;
//  }
{
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
  constexpr int full_row_width = 11 * 3 + 6;
  const auto output_status = [&](int iter_num) {
    std::cout
      << std::setfill('-') << std::setw(full_row_width) << '-' << '\n'
      << std::setfill(' ') << "Iter " << std::setw(full_row_width - 5) << iter_num << '\n'
      << std::setfill('-') << std::setw(full_row_width) << '-' << '\n'
      << std::setfill(' ')
        << std::setw(output_width) << "Actual" << " | "
        << std::setw(output_width) << "Estimated" << " | "
        << std::setw(output_width) << "Local func" << '\n'
      << std::setfill('-') << std::setw(output_width) << '-'
        << "-+-" << std::setw(output_width) << '-'
        << "-+-" << std::setw(output_width) << '-'
        << std::setfill(' ') << '\n'
      << std::setprecision(3) << std::fixed;
    for (std::size_t i = 0; i < real_solution.size(); ++i)
    {
      std::cout << std::setw(output_width) << real_solution[i]
        << " | " << std::setw(output_width) << global_solution[i]
        << " | " << std::setw(output_width) << target_function(linear_problems[i], global_solution) << '\n';
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
    const auto is_locally_converged = [&]() {
      for (std::size_t i = 0; i < global_solution.size(); ++i)
      {
        if (std::abs(global_solution[i] - local_solution[i]) >= convergence_criteria)
        {
          return false;
        }
      }
      return true;
    };
    bool is_converged;
    std::tie(global_solution, is_converged) = get_global_and_converged(local_solution, is_locally_converged());
    if (is_converged)
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
      5.0,
      0.000001
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

    admm_work(index, [&](const std::array<double, num_machines>& local_solution, const bool locally_converged) {
      // First value is to indicate convergence
      std::vector<double> to_send(num_machines + 1);
      // TODO: Allow sending different typed values so stuff like this can be avoided in the future
      //   This will require quite a lot of work / thinking about how to support it, though
      std::copy(local_solution.cbegin(), local_solution.cend(), to_send.begin() + 1);
      to_send.front() = locally_converged ? 1.0 : -1.0;
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
      // This has converged if this is the case
      const bool is_converged = new_global.front() > 0.0;
      std::array<double, 5> to_ret;
      std::copy(new_global.cbegin() + 1, new_global.cend(), to_ret.begin());
      return std::make_tuple(to_ret, is_converged);
    });

    ++counter;
    while (counter != num_machines)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
  });
  master.run();
}

void run_locally(const int index)
{
  static std::array<std::array<double, num_machines>, num_machines> local_solutions;
  static std::array<bool, num_machines> converged_array;
  static std::atomic<int> num_waiting{0};
  static std::mutex waiting_mutex;
  static std::atomic<int> working_on_data{0};
  static std::condition_variable cv;
  int num_waiting_needed = 0;
  admm_work(index, [&](const std::array<double, num_machines>& local_solution, const bool locally_converged) {
    num_waiting_needed += num_machines;
    // Ensure that the global data isn't touched while anything is working with it
    while (working_on_data > 0) { /* empty */ }
    std::unique_lock waiting_lock{waiting_mutex};
    local_solutions[index] = local_solution;
    converged_array[index] = locally_converged;
    ++num_waiting;
    if (num_waiting >= num_waiting_needed)
    {
      working_on_data = num_machines;
      cv.notify_all();
    }
    else
    {
      cv.wait(waiting_lock, [&]() { return num_waiting >= num_waiting_needed; });
    }
    waiting_lock.unlock();
    std::array<double, num_machines> global_solution;
    for (std::size_t i = 0; i < global_solution.size(); ++i)
    {
      // Sum each variable from each machine
      global_solution[i] = 0.0;
      for (std::size_t z = 0; z < local_solutions.size(); ++z)
      {
        global_solution[i] += local_solutions[z][i];
      }
      global_solution[i] /= num_machines;
    }
    const bool globally_converged = std::accumulate(converged_array.cbegin(), converged_array.cend(), true, std::bit_and<>{});
    --working_on_data;
    // std::cout << index << " !!! " << working_on_data << '\n';
    return std::make_tuple(global_solution, globally_converged);
  });
}

int main(const int argc, const char* const argv[])
{
  if (argc > 2)
  {
    std::cerr
      << "Usage:\n"
      << argv[0] << "[pass something to run without Skynet]\n";
    return 1;
  }
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i)
  {
    threads.emplace_back(argc == 1 ? machine_task : run_locally, i);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}
