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

constexpr std::uint16_t dummy_port = 55555;

template<typename Job, typename Container>
void process_wrapper(Job& job, const std::uint32_t id, const Container& c)
{
  job.process_data(id, c.data(), c.size());
}

TEST_CASE("Tag buffers work", "[Skynet_TagBuffers]")
{
  Master dummy{dummy_port, 0};
  Job<IntTag, DoubleTag> job{0, dummy};
  // Add an integer and a double (this is not what user code would look like)
  process_wrapper(job, 0, to_bytes(int_value));
  process_wrapper(job, 1, to_bytes(double_value));
  // Get back the values and ensure that they haven't changed
  REQUIRE(job.get<IntTag>() == int_value);
  REQUIRE(job.get<DoubleTag>() == double_value);
}

TEST_CASE("Tags can use repeated types", "[Skynet_TagRepeatedTypes]")
{
  Master dummy{dummy_port, 0};
  Job<IntTag, IntTag2> job{0, dummy};
  process_wrapper(job, 0, to_bytes(int_value));
  process_wrapper(job, 1, to_bytes(int_value2));
  REQUIRE(job.get<IntTag>() == int_value);
  REQUIRE(job.get<IntTag2>() == int_value2);
}