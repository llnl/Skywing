#include <catch2/catch.hpp>

#include "data/Skynet_WorkerThread.hpp"

#include <utility>

using namespace skynet;

TEST_CASE("Submitting tasks works", "[Skynet_Future_Submit]")
{
  WorkerThread worker;
  auto fut_one = worker.submit_work([]() { return true; });
  auto fut_two = worker.wrap_and_submit_work([]() { return std::make_pair(true, 1); });
  fut_one.wait(ErrorPlan::terminate_on_error);
  fut_two.wait(ErrorPlan::terminate_on_error);
  REQUIRE(fut_two.get(ErrorPlan::terminate_on_error) == 1);
}

TEST_CASE("Tasks needing multiple calls work", "[Skynet_Future_Multiple_Calls]")
{
  WorkerThread worker;
  auto fut_one = worker.submit_work([calls = 0]() mutable {
    ++calls;
    return calls >= 10;
  });
  fut_one.wait(ErrorPlan::terminate_on_error);
}
