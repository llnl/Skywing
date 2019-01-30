#include "catch2/catch.hpp"
#include "data/Skynet_Configuration.hpp"

TEST_CASE( "Configuration tests", "[Skynet_Configuration]" )
{
  std::ofstream outfile("test_config.txt");
  outfile << "A\t1" << std::endl;
  outfile << "B\t2" << std::endl;
  outfile << "C\t3" << std::endl;
  outfile.close();

  skynet::Configuration config("test_config.txt", "\t");

  SECTION( "test get_value when key exists" )
  {
    REQUIRE( config.get_value("A").compare("1") == 0 );
    REQUIRE( config.get_value("B").compare("2") == 0 );
    REQUIRE( config.get_value("C").compare("3") == 0 );
  }

  SECTION( "test has_key when key doesn't exist" )
  {
    REQUIRE( config.has_key("D") == false );
  }
}
