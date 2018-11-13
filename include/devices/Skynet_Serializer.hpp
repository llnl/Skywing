#ifndef SKYNET_SERIALIZER_HPP__
#define SKYNET_SERIALIZER_HPP__

#include <cstddef>
#include <type_traits>
#include <iterator>

namespace skynet
{
  template<typename T>
  void* serialize(const T& data)
  {
    throw std::runtime_error("serialize: Unknown type.");
  }

  template<>
  void* serialize(const int& data)
  {
    return static_cast<void*>(&data);
  }
  template<>
  void* serialize(const double& data)
  {
    return static_cast<void*>(&data);
  }
  template<>
  void* serialize(const unsigned& data)
  {
    return static_cast<void*>(&data);
  }
  template<>
  void* serialize(const bool& data)
  {
    return static_cast<void*>(&data);
  }

  template<typename S>
  void* serialize(const std::vector<S>& data)
  {
    return static_cast<void*>(data.data());
  }




  template<typename T>
  std::size_t get_serialized_size(const T&)
  {
    throw std::runtime_error("get_serialized_size: Unknown type.");
  }

  template<>
  std::size_t get_serialized_size(const int&)
  {
    return sizeof(int);
  }
  template<>
  std::size_t get_serialized_size(const double&)
  {
    return sizeof(double);
  }
  template<>
  std::size_t get_serialized_size(const unsigned&)
  {
    return sizeof(unsigned);
  }
  template<>
  std::size_t get_serialized_size(const bool&)
  {
    return sizeof(bool);
  }

  template<typename S>
  std::size_t get_serialized_size(const std::vector<S>& data)
  {
    return sizeof(S) * data.size();
  }


  

  template<typename T>
  T deserialize(std::vector<char>& data)
  {
    throw std::runtime_error("deserialize: Unknown type.");
  }

  template<>
  int deserialize(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing an int must have size 1.");
#endif
    return static_cast<int>(data[0]);
  }

  template<>
  double deserialize(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing a double must have size 1.");
#endif
    return static_cast<double>(data[0]);
  }

  template<>
  unsigned deserialize(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing an unsigned must have size 1.");
#endif
    return static_cast<unsigned>(data[0]);
  }

  template<>
  bool deserialize(std::vector<char>& data)
  {
#ifdef DEBUG
    if (data.size() != 1) 
      throw std::runtime_error("deserialize: deserializing a bool must have size 1.");
#endif
    return static_cast<bool>(data[0]);
  }

  template<typename S>
  template<>
  std::vector<S> deserialize(std::vector<char>& data)
  {
    std::vector<S> newVec;
    newVec.insert(newVec.end(), std::make_move_iterator(data.begin()), 
		  std::make_move_iterator(data.end()));
    return newVec;
  }
  

} // namespace skynet


#endif /* SKYNET_SERIALIZER_HPP__ */
