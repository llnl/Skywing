#ifndef SKYNET_SERIALIZABLE_HPP__
#define SKYNET_SERIALIZABLE_HPP__

#include <cstddef>

namespace skynet
{
  class Serializable
  {
  public:
    virtual void* serialize() = 0;
    virtual std::size_t get_serialized_size() = 0;
    virtual void clean_after_serialization() = 0;
  }; // class Serializable
} // namespace skynet

#endif /* SKYNET_SERIALIZABLE_HPP__ */
