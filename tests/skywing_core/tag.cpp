#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "skywing_core/tag.hpp"


TEST_CASE("Tags are equal", "[core]")
{
    skywing::Tag<int> tag0{"id0"};
    skywing::Tag<int> tag1{"id0"};
    REQUIRE(tag0 == tag1);
}

TEST_CASE("Tags are not equal by id", "[core]")
{
    skywing::Tag<int> tag0{"id0"};
    skywing::Tag<int> tag1{"id1"};
    REQUIRE(tag0 != tag1);
}

TEST_CASE("Tags less than", "[core]")
{
    skywing::Tag<int> tag0{"0"};
    skywing::Tag<int> tag1{"1"};
    REQUIRE(tag0 < tag1);
}

TEST_CASE("Tags greater than", "[core]")
{
    skywing::Tag<int> tag0{"0"};
    skywing::Tag<int> tag1{"1"};
    REQUIRE(tag1 > tag0);
}
