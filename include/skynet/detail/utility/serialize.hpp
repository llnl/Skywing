#ifndef SKYNET_UTILITY_SERIALIZE_HPP
#define SKYNET_UTILITY_SERIALIZE_HPP

#include <vector>
#include <sstream>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>

// The names to_bytes/from_bytes are used instead of serialize and deserialize
// as Cereal uses ADL on those names to find how to process data and that can
// cause ambiguity

namespace skynet { namespace detail
{
  /** \brief Turn a value into a std::vector<char>
   */
  template<typename T>
  std::vector<char> to_bytes(const T& t)
  {
    std::stringstream ss;
    cereal::PortableBinaryOutputArchive ar(ss);

    ar(t);

    auto s = ss.str();
    return {s.begin(), s.end()};
  }

  /** \brief Serializes a value to an already existing buffer
   *
   * \pre The buffer is large enough to hold the serialized value
   */
  template<typename T, typename OutputIterator>
  void to_bytes(const T& t, const OutputIterator it)
  {
    std::stringstream ss;
    cereal::PortableBinaryOutputArchive ar(ss);

    ar(t);

    auto s = ss.str();
    std::copy(s.begin(), s.end(), it);
  }

  /** \brief Turn raw data into a value
   */
  template<typename T>
  T from_bytes(const char* const data, const std::size_t size)
  {
    std::stringstream ss;
    ss.write(data, size);
    cereal::PortableBinaryInputArchive ar(ss);

    T t;
    ar(t);

    return t;
  }

  /** \brief Turn a ContiguousContainer into a value
   */
  template<typename T, typename ContiguousContainer>
  T from_bytes(const ContiguousContainer& data)
  {
    return from_bytes<T>(data.data(), data.size());
  }
} } // namespace skynet::detail

#endif // SKYNET_UTILITY_SERIALIZE_HPP
