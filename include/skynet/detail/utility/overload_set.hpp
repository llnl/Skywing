#ifndef SKYNET_DETAIL_UTILITY_OVERLOAD_SET_HPP
#define SKYNET_DETAIL_UTILITY_OVERLOAD_SET_HPP

#include <utility>

namespace skynet { namespace detail
{
  // C++17 version:
  // template<typename... Bases>
  // struct OverloadSet { using Bases::operator()...; };

  template<typename... Bases>
  struct OverloadSet;

  template<typename Base>
  struct OverloadSet<Base> : Base
  {
    OverloadSet(Base&& base)
      : Base{std::forward<Base>(base)}
    {}
    using Base::operator();
  };

  template<typename Base1, typename... Rest>
  struct OverloadSet<Base1, Rest...> : Base1, OverloadSet<Rest...>
  {
    OverloadSet(Base1&& base, Rest&&... rest)
      : Base1{std::forward<Base1>(base)}
      , OverloadSet<Rest...>{std::forward<Rest>(rest)...}
    {}
    using Base1::operator();
    using OverloadSet<Rest...>::operator();
  };

  /** \brief Creates an overload set from the passed Callables
   */
  template<typename... T>
  OverloadSet<T...> make_overload_set(T&&... callables) noexcept
  {
    return OverloadSet<T...>{std::forward<T>(callables)...};
  }
} } // namespace skynet::detail

#endif // SKYNET_DETAIL_UTILITY_OVERLOAD_SET_HPP
