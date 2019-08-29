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

// Roundtrip a std::vector<T> through serialize/deserialize
template<typename T>
std::vector<T> test_serialization(const std::vector<T>& t)
{
  return deserialize<std::vector<T>>(serialize(t));
}


TEST_CASE("POD Serialization/Deserialization works", "[Skynet_Serializer]")
{
  int test_int = -2;
  double test_double = 2.5;
  unsigned test_unsigned = 3;
  bool test_bool = true;
  std::vector<int> intvec { 3, 2, 9 };
  std::vector<double> doublevec { 3.2, 2.6, -9.3 };

  int recovered_int = test_serialization(test_int);
  double recovered_double = test_serialization(test_double);
  unsigned recovered_unsigned = test_serialization(test_unsigned);
  bool recovered_bool = test_serialization(test_bool);

  REQUIRE(test_int == recovered_int);
  REQUIRE(test_double == recovered_double);
  REQUIRE(test_unsigned == recovered_unsigned);
  REQUIRE(test_bool == recovered_bool);
}

TEST_CASE("Vector of POD Serialization/Deserialization works", "[Skynet_Serializer]")
{
  std::vector<int> intvec { 3, -2, 9 };
  std::vector<double> doublevec { 3.2, 2.6, -9.3 };
  std::vector<unsigned> unsignedvec { 3, 1, 11 };

  std::vector<int> recovered_intvec = test_serialization(intvec);
  std::vector<double> recovered_doublevec = test_serialization(doublevec);
  std::vector<unsigned> recovered_unsignedvec = test_serialization(unsignedvec);

  REQUIRE(intvec == recovered_intvec);
  REQUIRE(doublevec == recovered_doublevec);
  REQUIRE(unsignedvec == recovered_unsignedvec);
}
