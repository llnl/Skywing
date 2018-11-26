#include <Skynet_Serializer.hpp>
#include <iostream>
#include <cstring>
#include <catch.hpp>

using namespace skynet;

template < class T >
inline std::ostream& operator << (std::ostream& os, const std::vector<T>& v) 
{
    os << "[";
    for (typename std::vector<T>::const_iterator ii = v.begin(); ii != v.end(); ++ii)
    {
        os << " " << *ii;
    }
    os << " ]";
    return os;
}

template<typename T>
T test_serialization(T t)
{
  const void* p = serialize(t);
  //  std::cout << t << " " << *(static_cast<const T*>(p)) << std::endl;

  std::vector<char> charvec;
  if (charvec.size() < sizeof(t))
    charvec.resize(sizeof(t));

  std::memcpy(charvec.data(), p, sizeof(t));
  T recovered_val = deserialize<T>(charvec);
  //  std::cout << "Recovered data is " << recovered_val << std::endl << std::endl;
  return recovered_val;
}

template<typename S>
std::vector<S> test_serialization(std::vector<S> t)
{
  const void* p = serialize(t);
  // std::cout << t << " ";
  // for (unsigned i = 0; i < t.size(); i++)
  //   std::cout << *((const S*)(p) + i) << " ";
  // std::cout << std::endl;

  std::vector<char> charvec;
  if (charvec.size() < sizeof(S) * t.size())
    charvec.resize(sizeof(S) * t.size());

  std::memcpy(charvec.data(), p, sizeof(S) * t.size());
  std::vector<S> recovered_val = deserialize<std::vector<S>>(charvec);
  return recovered_val;
  //  std::cout << "Recovered data is " << recovered_val << std::endl << std::endl;
}

template<typename S>
bool compare_vecs(std::vector<S>& v1, std::vector<S> v2)
{
  if (v1.size() != v2.size()) return false;

  for (unsigned i = 0; i < v1.size(); i++)
    {
      if (v1[i] != v2[i]) return false;
    }
  return true;
}

TEST_CASE( "POD Serialization/Deserialization works", "[Skynet_Serializer]" )
{
  int test_int = -2;
  double test_double = 2.5;
  unsigned test_unsigned = 3;
  bool test_bool = true;
  std::vector<int> intvec { 3, 2, 9 };
  std::vector<double> doublevec {3.2, 2.6, -9.3};

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
  std::vector<double> doublevec {3.2, 2.6, -9.3};
  std::vector<unsigned> unsignedvec { 3, 1, 11 };

  std::vector<int> recovered_intvec = test_serialization(intvec);
  std::vector<double> recovered_doublevec = test_serialization(doublevec);
  std::vector<unsigned> recovered_unsignedvec = test_serialization(unsignedvec);

  REQUIRE(compare_vecs(intvec, recovered_intvec));
  REQUIRE(compare_vecs(doublevec, recovered_doublevec));
  REQUIRE(compare_vecs(unsignedvec, recovered_unsignedvec));
}

// int main()
// {
//   int test_int = -2;
//   double test_double = 2.5;
//   unsigned test_unsigned = 3;
//   bool test_bool = true;
//   std::vector<int> intvec { 3, 2, 9 };
//   std::vector<double> doublevec {3.2, 2.6, -9.3};

//   test_serialization(test_int);
//   test_serialization(test_double);
//   test_serialization(test_unsigned);
//   test_serialization(test_bool);
//   test_serialization(intvec);
//   test_serialization(doublevec);
    
//   return 0;
// }
