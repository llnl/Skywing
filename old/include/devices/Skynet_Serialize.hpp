#ifndef SKYNET_SERIALIZE_HPP__
#define SKYNET_SERIALIZE_HPP__

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <iterator>
#include <memory>
#include <vector>
#include <cstring>
#include <iostream>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>

namespace skynet
{
  /** \brief Turn a value into a std::vector<char> */
  template<typename T>
  std::vector<char> serialize(const T& t)
  {
    std::stringstream ss;
    cereal::PortableBinaryOutputArchive ar(ss);

    ar(t);

    auto s = ss.str();
    return {s.begin(), s.end()};
  }

  /** \brief Turn a std::vector<char> into a value */
  template<typename T>
  T deserialize(const std::vector<char>& data)
  {
    std::stringstream ss;
    ss.write(data.data(), data.size());
    cereal::PortableBinaryInputArchive ar(ss);

    T t;
    ar(t);

    return t;
  }

} // namespace skynet


#endif /* SKYNET_SERIALIZE_HPP__ */
