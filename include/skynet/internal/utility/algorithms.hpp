#ifndef SKYNET_INTERNAL_UTILITY_ALGORITHMS
#define SKYNET_INTERNAL_UTILITY_ALGORITHMS

#include <type_traits>

namespace skynet::internal
{
  /** \brief Concatenates many vectors into a single vector
   */
  template<typename... Ts>
  std::common_type_t<Ts...> concatenate(const Ts&... vecs)
  {
    std::common_type_t<Ts...> to_ret((size(vecs) + ...));
    // Use a pointer instead of references since it'll copy otherwise
    auto copy_loc = begin(to_ret);
    for (const auto& vec : {&vecs...})
    {
      copy_loc = std::copy(cbegin(vec), cend(vec), copy_loc);
    }
    return to_ret;
  }
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_ALGORITHMS
