#include <catch2/catch.hpp>

#include <capnp/message.h>

#include "skynet_core/internal/capn_proto_wrapper.hpp"

#include "../src/skynet_core/include/publish_value_handler.hpp"

using namespace skynet::internal;

template<typename T>
bool roundtrip_value(const T& val)
{
  // Create the message
  capnp::MallocMessageBuilder message;
  auto builder = message.initRoot<cpnpro::PublishData>().initValue();
  using handler = detail::PublishValueHandler<T>;
  handler::set(builder, val);

  // Extract the value from the message and check equivalence
  auto value = handler::get(builder);
  REQUIRE(value);
  return *value == val;
}

TEST_CASE("Cap'n Proto Wrappers Work", "[Skynet_CapnProto_Wrappers]")
{
  using namespace std::string_literals;

  REQUIRE(roundtrip_value(std::int16_t{10}));
  REQUIRE(roundtrip_value(10.0));
  REQUIRE(roundtrip_value(std::vector<std::int8_t>{1, 2, 3}));
  REQUIRE(roundtrip_value("test a string"s));
  REQUIRE(roundtrip_value(std::vector<std::string>{"str1", "str2"}));
  REQUIRE(roundtrip_value(std::vector<std::byte>{std::byte{0x10}, std::byte{0x80}, std::byte{0x7F}}));
}
