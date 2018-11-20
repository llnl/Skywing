#ifndef SKYNET_SERIALIZER_HPP__
#define SKYNET_SERIALIZER_HPP__

#include <cstddef>
#include <type_traits>
#include <iterator>
#inlcude <memory>
#include "Skynet_Serializable.hpp"

namespace skynet
{
  const void* serialize(const int& data)
  {
    return static_cast<const void*>(&data);
  }



<<<<<<< HEAD
  const void* serialize(const double& data)
  {
    return static_cast<const void*>(&data);
=======

  // template<>
  void* serialize(const int& data)
  {
    return static_cast<void*>((void *) &data);
>>>>>>> 498329fd7c2482ce1075e469712614fd1345738b
  }
  const void* serialize(const unsigned& data)
  {
<<<<<<< HEAD
    return static_cast<const void*>(&data);
=======
    return static_cast<void*>((void *) &data);
>>>>>>> 498329fd7c2482ce1075e469712614fd1345738b
  }
  const void* serialize(const bool& data)
  {
<<<<<<< HEAD
    return static_cast<const void*>(&data);
=======
    return static_cast<void*>((void *) &data);
>>>>>>> 498329fd7c2482ce1075e469712614fd1345738b
  }

  template<typename S>
  const void* serialize(const std::vector<S>& data)
  {
    return static_cast<const void*>(data.data());
  }


=======
    return static_cast<void*>((void *) &data);
>>>>>>> 498329fd7c2482ce1075e469712614fd1345738b
  }

  std::vector<char> serialize(const Serializable* data)
  {
<<<<<<< HEAD
    return data.serialize();
=======
    return static_cast<void*>((void *) data.data());
>>>>>>> 498329fd7c2482ce1075e469712614fd1345738b
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
  T deserialize(std::vector<char>& data)
  {
    return T::deserialize(data);
  }

  template<>
  int deserialize<int>(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing an int must have size 1.");
#endif
    return *static_cast<int*>(data.data());
  }

  template<>
  double deserialize<double>(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing a double must have size 1.");
#endif
    return *static_cast<double*>(data.data());
  }

  template<>
<<<<<<< HEAD
  unsigned deserialize<unsigned>(std::vector<char>& data)
=======
  char deserialize(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing a char must have size 1.");
#endif
    return static_cast<char>(data[0]);
  }

  template<>
  unsigned deserialize(std::vector<char>& data)
>>>>>>> 498329fd7c2482ce1075e469712614fd1345738b
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing an unsigned must have size 1.");
#endif
    return *static_cast<unsigned*>(data.data());
  }

  template<>
  bool deserialize<bool>(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing a bool must have size 1.");
#endif
    return *static_cast<bool*>(data.data());
  }

<<<<<<< HEAD
  template<typename S>
  template<>
  std::vector<S> deserialize<std::vector<S>>(std::vector<char>& data)
  {
    std::vector<S> newVec;
    newVec.insert(newVec.end(), std::make_move_iterator(data.begin()), 
		  std::make_move_iterator(data.end()));
    return newVec;
  }
=======
  // template<typename S>
  // std::vector<S> deserialize(std::vector<char>& data)
  // {
  //   std::vector<S> newVec;
  //   newVec.insert(newVec.end(), std::make_move_iterator(data.begin()), 
    //   std::make_move_iterator(data.end()));
  //   return newVec;
  // }
>>>>>>> 498329fd7c2482ce1075e469712614fd1345738b
  
  
} // namespace skynet


#endif /* SKYNET_SERIALIZER_HPP__ */
