#ifndef SKYNET_UTILITY_OPTIONAL_HPP
#define SKYNET_UTILITY_OPTIONAL_HPP

#include <array>
#include <new>

namespace skynet
{
  /** \brief A class that represents an optional value.
   *
   * There don't seem to be any good options outside of Boost, which relies
   * on many other parts of Boost.  If this project is ever moved to use C++17
   * or Boost is included, they should be used instead.  This has a subset of
   * the functionality offered by them so it should be a drop-in replacement.
   */
  template<typename T>
  class Optional
  {
  public:
    /** \brief The type the optional hold
     */
    using value_type = T;

    /** \brief Construct an optional that holds no value
     */
    constexpr Optional() noexcept
      : has_value_{false}
    {}

    /** \brief Construct an optional from an already existing value
     */
    constexpr Optional(const T& value) noexcept
      : has_value_{true}
      , value_{value}
    {}

    /** \brief Construct an optional by moving from a value
     */
    constexpr Optional(T&& value) noexcept
      : has_value_{true}
      , value_{std::move(value)}
    {}

    // Move constructor/assignment operator
    Optional(Optional&& other) noexcept
      : has_value_{other.has_value_}
    {
      // only need to move it if there's a value
      if (has_value_)
      {
        value_ = std::move(other.value_);
      }
      // NOTE: Do NOT set other.has_value_ to false since it still holds
      //       a value; it's just been moved from
    }

    Optional& operator=(Optional&& other) noexcept
    {
      // No need for a self-assignment check since self-move assignment needs
      // to be valid; don't reset other.has_value_ as it still holds a
      // (moved from) value
      has_value_ = other.has_value_;
      value_ = std::move(other.value_);
      return *this;
    }

    /** \brief Destructor
     */
    ~Optional()
    {
      // Only call the destructor if there's a value
      if (has_value_)
      {
        value_.~T();
      }
    }

    // Disable copying until there's a good reason to enable it
    Optional(const Optional&) = delete;
    Optional& operator=(const Optional&) = delete;

    /** \brief Returns a reference to the value
     *
     * Behavior is undefined if the optional holds no value.
     */
    T& operator*() & noexcept
    {
      return value_;
    }
    const T& operator*() const& noexcept
    {
      return value_;
    }
    T&& operator*() && noexcept
    {
      // "this" is always an lvalue so this will call the lvalue version
      return std::move(value_);
    }
    const T&& operator*() const&& noexcept
    {
      return std::move(value_);
    }

    /** \brief Returns a pointer to the contained value
     */
    T* operator->() noexcept
    {
      return std::addressof(value_);
    }
    const T* operator->() const noexcept
    {
      return std::addressof(value_);
    }

    /** \brief Returns the contained value if the optional hold a value,
     * otherwise returns the passed value
     */
    template<typename U>
    T value_or(U&& default_value) const&
    {
      return static_cast<bool>(*this)
        ? value_
        : static_cast<T>(std::forward<U>(default_value));
    }
    template<typename U>
    T value_or(T&& default_value) &&
    {
      return static_cast<bool>(*this)
        ? std::move(value_)
        : static_cast<T>(std::forward<U>(default_value));
    }

    /** \brief Returns true if the optional holds a value, false otherwise
     */
    constexpr explicit operator bool() const noexcept
    {
      return has_value_;
    }

  private:
    bool has_value_;
    union
    {
      // dummy value to allow not forcing construction of the real value
      char dummy_;
      T value_;
    };
  }; // class optional
}; // namespace skynet

#endif // SKYNET_UTILITY_OPTIONAL_HPP
