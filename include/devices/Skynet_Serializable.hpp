#ifndef SKYNET_SERIALIZABLE_HPP__
#define SKYNET_SERIALIZABLE_HPP__

#include <cstddef>

namespace skynet
{
  class Serializable
  {
  public:
    virtual std::vector<char> serialize() = 0;

  }; // class Serializable
} // namespace skynet

#endif /* SKYNET_SERIALIZABLE_HPP__ */
