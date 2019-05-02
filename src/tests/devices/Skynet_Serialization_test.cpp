#include "catch2/catch.hpp"
#include "devices/Skynet_Serialize.hpp"
#include <iostream>
#include <cstring>

using namespace skynet;

template < class T >
inline std::ostream& operator << (std::ostream& os, const std::vector<T>& v)
{
    os << "[ ";
    for (auto&& el : v)
    {
        os << el << " ";
    }
    os << "]";
    return os;
}

// Roundtrip a value through serialize/deserialize
template<typename T>
T test_serialization(T t)
{
  const auto p = serialize(t);

  std::vector<char> charvec(p.size());

  std::memcpy(charvec.data(), p.data(), p.size());

  T recovered_val = deserialize<T>(charvec);

  return recovered_val;
}

// Roundtrip a std::vector<S> through serialize/deserialize
template<typename S>
std::vector<S> test_serialization(std::vector<S> t)
{
  const auto p = serialize(t);

  std::vector<char> charvec(p.size());

  std::memcpy(charvec.data(), p.data(), p.size());

  std::vector<S> recovered_val = deserialize<std::vector<S>>(charvec);
  return recovered_val;
}


TEST_CASE( "POD Serialization/Deserialization works", "[Skynet_Serializer]" )
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

TEST_CASE( "Vector of POD Serialization/Deserialization works", "[Skynet_Serializer]" )
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
