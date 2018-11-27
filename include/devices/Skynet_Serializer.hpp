#ifndef SKYNET_SERIALIZER_HPP__
#define SKYNET_SERIALIZER_HPP__

#include <cstddef>
#include <type_traits>
#include <iterator>
#include <memory>
#include <vector>
#include <cstring>
#include <iostream>
#include "Skynet_Serializable.hpp"

namespace skynet
{
  const void* serialize(const int& data)
  {
    return static_cast<const void*>(&data);
  }
  const void* serialize(const double& data)
  {
    return static_cast<const void*>(&data);
  }
  const void* serialize(const unsigned& data)
  {
    return static_cast<const void*>(&data);
  }
  const void* serialize(const bool& data)
  {
    return static_cast<const void*>(&data);
  }

  template<typename S>
  const void* serialize(const std::vector<S>& data)
  {
    static_assert(not std::is_same<S, bool>::value, 
		  "serialize: std::vector<bool> is not necessarily byte-packed,"
		  " so we're not currently supporting its serialization.");
    return static_cast<const void*>(data.data());
  }

  std::vector<char> serialize(const Serializable& data)
  {
    return data.serialize();
  }




  const void* convert_if_vec(const void* p)
  { return p; }

  template<typename T>
  const void* convert_if_vec(const std::vector<char>& p)
  {
    return static_cast<const void*>(p.data());
  }



  std::size_t get_serialized_size(const int&, const void*)
  {
    return sizeof(int);
  }

  std::size_t get_serialized_size(const double&, const void*)
  {
    return sizeof(double);
  }

  std::size_t get_serialized_size(const unsigned&, const void*)
  {
    return sizeof(unsigned);
  }

  std::size_t get_serialized_size(const bool&, const void*)
  {
    return sizeof(bool);
  }

  template<typename S>
  std::size_t get_serialized_size(const std::vector<S>& data, const void*)
  {
    return sizeof(S) * data.size();
  }

  std::size_t get_serialized_size(const Serializable&, const std::vector<char>& pData)
  {
    return sizeof(char) * pData.size();
  }





  template<typename T>
  struct deserializeImplClass;

  template<typename T>
  T deserialize(const std::vector<char>& data)
  {
    return deserializeImplClass<T>::deserializeImpl(data);
  }
  template<typename T>
  T deserialize(const char* data)
  {
    return deserializeImplClass<T>::deserializeImpl(data);
  }


  // Generic template to be used when passed something of type Serializable
  template<typename T>
  struct deserializeImplClass
  {
    static T deserializeImpl(const std::vector<char>& data)
    {
      return T::deserialize(data);
    }
  };


  template<>
  struct deserializeImplClass<int>
  {
    static int deserializeImpl(const std::vector<char>& data)
    { return *(reinterpret_cast<const int*>(data.data())); }

    static int deserializeImpl(const char* data)
    { return *(reinterpret_cast<const int*>(data)); }
  };

  template<>
  struct deserializeImplClass<unsigned>
  {
    static unsigned deserializeImpl(const std::vector<char>& data)
    { return *(reinterpret_cast<const unsigned*>(data.data())); }

    static unsigned deserializeImpl(const char* data)
    { return *(reinterpret_cast<const unsigned*>(data)); }
  };

  template<>
  struct deserializeImplClass<double>
  {
    static double deserializeImpl(const std::vector<char>& data)
    { return *(reinterpret_cast<const double*>(data.data())); }

    static double deserializeImpl(const char* data)
    { return *(reinterpret_cast<const double*>(data)); }
  };

  template<>
  struct deserializeImplClass<bool>
  {
    static bool deserializeImpl(const std::vector<char>& data)
    { return *(reinterpret_cast<const bool*>(data.data())); }

    static bool deserializeImpl(const char* data)
    { return *(reinterpret_cast<const bool*>(data)); }
  };
    
  template<typename S>
  struct deserializeImplClass<std::vector<S>>
  {
    static std::vector<S> deserializeImpl(const std::vector<char>& data)
    {
      std::vector<S> newVec(data.size() / sizeof(S));
      for (unsigned i = 0; i < newVec.size(); i++)
	  newVec[i] = deserialize<S>(data.data() + i * sizeof(S));
      return newVec;
    }
  };  
  
} // namespace skynet


#endif /* SKYNET_SERIALIZER_HPP__ */
