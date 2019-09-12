#include <catch2/catch.hpp>

#include "skynet/job.hpp"
#include "skynet/utility/serialize.hpp"

#include <vector>
#include <cstring>

using namespace skynet;

struct IntTag : Tag<int> {};
struct IntTag2 : Tag<int> {};
struct DoubleTag : Tag<double> {};

constexpr int int_value = 10;
constexpr int int_value2 = 20;
constexpr double double_value = 14.64;

TEST_CASE("Tag buffers work", "[Skynet_TagBuffers]")
{
  Job<IntTag, DoubleTag> job;
  // Add an integer and a double (this is not what user code would look like)
  job.process_data(0, serialize(int_value));
  job.process_data(1, serialize(double_value));
  // Get back the values and ensure that they haven't changed
  REQUIRE(job.get_value<IntTag>() == int_value);
  REQUIRE(job.get_value<DoubleTag>() == double_value);
}

TEST_CASE("Tags can use repeated types", "[Skynet_TagRepeatedTypes]")
{
  Job<IntTag, IntTag2> job;
  job.process_data(0, serialize(int_value));
  job.process_data(1, serialize(int_value2));
  REQUIRE(job.get_value<IntTag>() == int_value);
  REQUIRE(job.get_value<IntTag2>() == int_value2);
}