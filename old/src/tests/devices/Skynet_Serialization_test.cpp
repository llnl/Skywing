#include "catch2/catch.hpp"
#include "devices/Skynet_Serialize.hpp"
#include <iostream>
#include <cstring>

using namespace skynet;

// Roundtrip a value through serialize/deserialize
template<typename T>
T test_serialization(const T& t)
{
  return deserialize<T>(serialize(t));
}

TEST_CASE("POD Serialization/Deserialization works", "[Skynet_Serializer]")
{
  const int test_int = -2;
  const double test_double = 2.5;
  const unsigned test_unsigned = 3;
  const bool test_bool = true;
  const std::vector<int> intvec{3, 2, 9};
  const std::vector<double> doublevec{3.2, 2.6, -9.3};

  const int recovered_int = test_serialization(test_int);
  const double recovered_double = test_serialization(test_double);
  const unsigned recovered_unsigned = test_serialization(test_unsigned);
  const bool recovered_bool = test_serialization(test_bool);

  REQUIRE(test_int == recovered_int);
  REQUIRE(test_double == recovered_double);
  REQUIRE(test_unsigned == recovered_unsigned);
  REQUIRE(test_bool == recovered_bool);
}

TEST_CASE("Vector of POD Serialization/Deserialization works", "[Skynet_Serializer]")
{
  const std::vector<int> intvec{3, -2, 9};
  const std::vector<double> doublevec{3.2, 2.6, -9.3};
  const std::vector<unsigned> unsignedvec {3, 1, 11};

  const std::vector<int> recovered_intvec = test_serialization(intvec);
  const std::vector<double> recovered_doublevec = test_serialization(doublevec);
  const std::vector<unsigned> recovered_unsignedvec = test_serialization(unsignedvec);

  REQUIRE(intvec == recovered_intvec);
  REQUIRE(doublevec == recovered_doublevec);
  REQUIRE(unsignedvec == recovered_unsignedvec);
}
