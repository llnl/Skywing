#ifndef SKYNET_INTERNAL_UTILITY_ALGORITHMS
#define SKYNET_INTERNAL_UTILITY_ALGORITHMS

#include <iterator>
#include <type_traits>
#include <vector>

namespace skynet::internal
{
  /** \brief Concatenates many containers into a single vector
   */
  template<typename... Ts>
  std::vector<std::common_type_t<typename Ts::value_type...>> concatenate(const Ts&... containers) noexcept
  {
    // Allocate enough space for all the data at the start
    std::vector<std::common_type_t<typename Ts::value_type...>> to_ret((std::size(containers) + ...));
    // Copy all of the data
    auto copy_loc = begin(to_ret);
    ((copy_loc = std::copy(std::cbegin(containers), std::cend(containers), copy_loc)), ...);
    return to_ret;
  }
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_ALGORITHMS
