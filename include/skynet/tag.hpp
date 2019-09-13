#ifndef SKYNET_TAG_HPP
#define SKYNET_TAG_HPP

#include <vector>

#include "utility/serialize.hpp"

namespace skynet
{
  /** \brief A tag for sending values
   *
   * Tags certain values to be sent; all values sent for a specific tag must
   * be of the same type.  This type should not be constructed by user code
   * (just inherited) so the constructor is protected.
   */
  template <typename T>
  class Tag
  {
  public:
    /** \brief Parses raw data and appends it to a vector
     *
     * This shouldn't be publicly exposed.
     * \param data The data to parse to add the the vector
     * \param append_to The vector to append to, must be of type std::vector<T>
     */
    static void append_to_queue(
      const char* const data,
      const std::size_t size,
      void* const append_to
    )
    {
      auto* const true_append_to = static_cast<std::vector<T>*>(append_to);
      true_append_to->push_back(from_bytes<T>(data, size));
    }

    /** \brief The type being sent over this tag
     */
    using value_type = T;

  protected:
    Tag() = default;
  }; // class Tag
} // namespace skynet

#endif // SKYNET_TAG_HPP
