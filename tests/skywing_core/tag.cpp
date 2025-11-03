#include "skywing_core/tag.hpp"

#include <iostream>
#include <vector>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Tags can be compared", "[core]")
{
    skywing::Tag<double> tag0{"0"};
    skywing::Tag<double> tag1{"1"};

    SECTION("Tags are equal")
    {
        skywing::Tag<double> twin{"0"};
        REQUIRE(tag0 == twin);
    }
    SECTION("Tags are not equal by id")
    {
        REQUIRE(tag0 != tag1);
    }
    SECTION("Tag is less than")
    {
        REQUIRE(tag0 < tag1);
    }
    SECTION("Tag is greater than")
    {
        REQUIRE(tag1 > tag0);
    }
}

TEST_CASE("Tag creation", "[core]")
{
    skywing::Tag<float, double, bool> tag{"id"};
    std::vector<std::uint8_t> expected_types{0, 2, 23};
    REQUIRE(tag.id() == "id");
    REQUIRE(tag.get_expected_types() == expected_types);
}
