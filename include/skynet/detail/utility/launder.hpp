#ifndef SKYNET_UTILITY_LAUNDER_HPP
#define SKYNET_UTILITY_LAUNDER_HPP

#if __cplusplus >= 201703L
  #include <new>
#endif

namespace skynet { namespace detail
{
  // This is a simple wrapper for defining std::launder if it exists,
  // which explicitly makes some undefined (but generally working) behavior
  // into well-defined behavior
  #if __cplusplus >= 201703L
    template<class T>
    [[nodiscard]] constexpr T* launder(T* p) noexcept
    {
      return std::launder(p);
    }
  #else
    template<class T>
    constexpr T* launder(T* p) noexcept
    {
      return p;
    }
  #endif
} } // namespace skynet::detail

#endif // SKYNET_UTILITY_LAUNDER_HPP
