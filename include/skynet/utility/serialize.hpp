#ifndef SKYNET_UTILITY_SERIALIZE_HPP
#define SKYNET_UTILITY_SERIALIZE_HPP

#include <vector>
#include <sstream>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>

namespace skynet
{
  /** \brief Turn a value into a std::vector<char>
   */
  template<typename T>
  std::vector<char> serialize(const T& t)
  {
    std::stringstream ss;
    cereal::PortableBinaryOutputArchive ar(ss);

    ar(t);

    auto s = ss.str();
    return {s.begin(), s.end()};
  }

  /** \brief Turn a std::vector<char> into a value
   */
  template<typename T, typename ContiguousContainer>
  T deserialize(const ContiguousContainer& data)
  {
    std::stringstream ss;
    ss.write(data.data(), data.size());
    cereal::PortableBinaryInputArchive ar(ss);

    T t;
    ar(t);

    return t;
  }

} // namespace skynet


#endif // SKYNET_UTILITY_SERIALIZE_HPP
