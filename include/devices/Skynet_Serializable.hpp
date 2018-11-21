#ifndef SKYNET_SERIALIZABLE_HPP__
#define SKYNET_SERIALIZABLE_HPP__

#include <cstddef>
#include <vector>

namespace skynet
{
  class Serializable
  {
  public:
    // You must also implement the following static function:
    // static Derived deserialize(const std::vector<char>& data);

    virtual std::vector<char> serialize() const = 0;
  }; // class Serializable
} // namespace skynet

#endif /* SKYNET_SERIALIZABLE_HPP__ */
