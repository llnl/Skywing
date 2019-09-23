#ifndef SKYNET_INTERNAL_UTILITY_OVERLOAD_SET_HPP
#define SKYNET_INTERNAL_UTILITY_OVERLOAD_SET_HPP

#include <utility>

namespace skynet::internal
{
  /** \brief A struct to create and overload set from unrelated function objects
   */
  template<typename... Bases>
  struct OverloadSet : Bases...
  {
    OverloadSet(Bases&&... bases) noexcept
      : Bases{std::forward<Bases>(bases)}...
    {}

    // Bring all the call operators in
    using Bases::operator()...;
  };

  /** \brief Creates an overload set from the passed Callables
   */
  template<typename... T>
  OverloadSet<T...> make_overload_set(T&&... callables) noexcept
  {
    return OverloadSet<T...>{std::forward<T>(callables)...};
  }
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_OVERLOAD_SET_HPP
