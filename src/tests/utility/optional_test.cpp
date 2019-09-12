#include <catch2/catch.hpp>

#include "skynet/utility/optional.hpp"

#include <utility>

using namespace skynet;

TEST_CASE("Optional works", "[Skynet_Optional]")
{
  Optional<int> a;
  REQUIRE_FALSE(a);
  Optional<char> b{'b'};
  REQUIRE(b);
  REQUIRE(*b == 'b');
}

// Have a wrapper function since Clang will warn against self-assignment
template<typename T>
void assign(Optional<T>& a, Optional<T>& b)
{
  a = std::move(b);
}

TEST_CASE("Optional self-assignment works", "[Skynet_OptionalSelfAssign]")
{
  Optional<int> a;
  assign(a, a);
  REQUIRE_FALSE(a);
  Optional<char> b{'b'};
  assign(b, b);
  REQUIRE(b);
  REQUIRE(*b == 'b');
}
