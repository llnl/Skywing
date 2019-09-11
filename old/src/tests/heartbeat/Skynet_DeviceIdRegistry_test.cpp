#include <arpa/inet.h>
#include "catch2/catch.hpp"
#include "heartbeat/Skynet_DeviceIdRegistry.hpp"

TEST_CASE("DeviceIdRegistry<uint8_t> tests", "[Skynet_DeviceIdRegistry]")
{
  skynet::DeviceIdRegistry<uint8_t> registry;

  SECTION("register and check first 3 values")
  {
    REQUIRE(registry.next_id() == 0);
    REQUIRE(registry.next_id() == 1);
    REQUIRE(registry.next_id() == 2);
  }

  SECTION("register 3 values, free value 1, and re-register value 1")
  {
    registry.next_id();
    registry.next_id();
    registry.next_id();
    registry.free_id(1);
    REQUIRE(registry.next_id() == 1);
  }

  SECTION("check loop around")
  {
    for(uint8_t i=3; i < UINT8_MAX; ++i)
      registry.next_id();
    registry.free_id(1);
    REQUIRE(registry.next_id() == 1);
  }
}
