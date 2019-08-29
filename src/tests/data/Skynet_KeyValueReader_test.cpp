#include "catch2/catch.hpp"
#include "data/Skynet_KeyValueReader.hpp"

TEST_CASE("KeyValueReader tests", "[Skynet_KeyValueReader]")
{
  {
    std::ofstream outfile("test_config.txt");
    outfile
      << "A\t1\n"
      << "B\t2\n"
      << "C\t3\n";
  }

  const skynet::KeyValueReader config("test_config.txt", "\t");

  SECTION("test get_value when key exists")
  {
    REQUIRE(config.get_value("A") == "1");
    REQUIRE(config.get_value("B") == "2");
    REQUIRE(config.get_value("C") == "3");
  }

  SECTION("test has_key when key doesn't exist")
  {
    REQUIRE_FALSE(config.key_exists("D"));
  }
}
