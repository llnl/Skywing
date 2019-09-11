#include <catch2/catch.hpp>

#include "skynet/utility/optional.hpp"

using namespace skynet;

TEST_CASE("Optional works", "[Skynet_Optional]")
{
  Optional<int> a;
  REQUIRE(!a);
  Optional<char> b('b');
  REQUIRE(b);
  REQUIRE(*b == 'b');
}
