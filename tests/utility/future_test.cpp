#include <catch2/catch.hpp>

#include "skynet/utility/worker_thread.hpp"

#include <utility>

using namespace skynet;

TEST_CASE("Submitting tasks works", "[Skynet_FutureSubmit]")
{
  WorkerThread worker;
  auto fut_one = worker.submit_work([]() { return true; });
  auto fut_two = worker.wrap_and_submit_work([]() { return Optional<int>{1}; });
  fut_one.wait();
  fut_two.wait();
  REQUIRE(fut_two.get() == 1);
}

TEST_CASE("Tasks needing multiple calls work", "[Skynet_FutureMultipleCalls]")
{
  WorkerThread worker;
  auto fut_one = worker.submit_work([calls = 0]() mutable {
    ++calls;
    return calls >= 10;
  });
  fut_one.wait();
}
