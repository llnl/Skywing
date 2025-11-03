#include "skywing_core/include/publish_value_handler.hpp"
#include "skywing_core/internal/capn_proto_wrapper.hpp"
#include <capnp/message.h>
#include <catch2/catch_test_macros.hpp>

using namespace skywing::internal;

namespace
{
template <typename T>
bool roundtrip_value(const T& val)
{
    // Create the message
    capnp::MallocMessageBuilder message;
    auto builder = message.initRoot<cpnpro::PublishData>().initValue(1)[0];
    using handler = detail::PublishValueHandler<T>;
    handler::set(builder, val);

    // Extract the value from the message and check equivalence
    auto value = handler::get(builder);
    REQUIRE(value);
    return *value == val;
}
} // namespace

TEST_CASE("Cap'n Proto Wrappers Work", "[core][unit]")
{
    using namespace std::string_literals;

    REQUIRE(roundtrip_value(std::int16_t{10}));
    REQUIRE(roundtrip_value(10.0));
    REQUIRE(roundtrip_value(std::vector<std::int8_t>{1, 2, 3}));
    REQUIRE(roundtrip_value("test a string"s));
    REQUIRE(roundtrip_value(std::vector<std::string>{"str1", "str2"}));
    REQUIRE(roundtrip_value(std::vector<std::byte>{
        std::byte{0x10}, std::byte{0x80}, std::byte{0x7F}}));
}
