#ifndef SKYWING_TYPES_HPP
#define SKYWING_TYPES_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "skywing_core/internal/utility/type_list.hpp"

namespace skywing
{
/// The ID type for machines
using MachineID = std::string;

/// The ID type for jobs
using JobID = std::string;

/// The ID type for message versions.
using VersionID = std::uint32_t;

/// The ID type for tags
using TagID = std::string;

/// The type used for communicating message sizes over the network
using NetworkSizeType = std::uint32_t;

/// A typelist of all the types that can be published
using PublishValueTypeList =
    internal::TypeList<float,
                       std::vector<float>,
                       double,
                       std::vector<double>,
                       std::int8_t,
                       std::vector<std::int8_t>,
                       std::int16_t,
                       std::vector<std::int16_t>,
                       std::int32_t,
                       std::vector<std::int32_t>,
                       std::int64_t,
                       std::vector<std::int64_t>,
                       std::uint8_t,
                       std::vector<std::uint8_t>,
                       std::uint16_t,
                       std::vector<std::uint16_t>,
                       std::uint32_t,
                       std::vector<std::uint32_t>,
                       std::uint64_t,
                       std::vector<std::uint64_t>,
                       std::string,
                       std::vector<std::string>,
                       std::vector<std::byte>,
                       bool,
                       // TODO: std::vector<bool> is awful, but don't really
                       // want an exception either...
                       std::vector<bool>>;

/// Variant version of the above
using PublishValueVariant =
    internal::ApplyTo<PublishValueTypeList, std::variant>;

namespace internal::detail
{
// Checks if a span representing a tag's value is valid compared to what is
// expected
template <typename... Ts, std::size_t... Is>
bool span_is_valid(const std::span<const PublishValueVariant> value,
                   std::index_sequence<Is...>) noexcept
{
    return value.size() == sizeof...(Ts)
           && (...
               && (value[Is].index() == index_of<Ts, PublishValueTypeList>) );
}

// Can't use std::conditional_t because of At not working for 0 size packs
// but conditional_t requires both types to be well-formed
template <typename... Ts>
struct ValueOrTupleImpl
{
    using Type = std::tuple<Ts...>;
};
template <typename T>
struct ValueOrTupleImpl<T>
{
    using Type = T;
};
} // namespace internal::detail

/// Takes a parameter pack and either packs it into a tuple or
/// turns it into a single type
template <typename... Ts>
using ValueOrTuple = typename internal::detail::ValueOrTupleImpl<Ts...>::Type;

namespace internal::detail
{
// Takes a tag value and turns it into either a value (single element) or tuple
// non-const version
template <typename... Ts, std::size_t... Is>
ValueOrTuple<Ts...> make_value(std::span<PublishValueVariant> value,
                               std::index_sequence<Is...> seq) noexcept
{
    (void) seq; // avoid compiler warning in release buiild
    //assert(span_is_valid<Ts...>(value, seq));
    if constexpr (sizeof...(Ts) == 1) {
        assert(std::get_if<Ts...>(&value[0]));
        return std::move(*std::get_if<Ts...>(&value[0]));
    }
    else {
        // Is lines up for the span, Ts is the types for the tuple
        assert((... && std::get_if<Ts>(&value[Is])));
        return std::make_tuple(std::move(*std::get_if<Ts>(&value[Is]))...);
    }
}

// Const version of the above
template <typename... Ts, std::size_t... Is>
ValueOrTuple<Ts...> make_value(std::span<const PublishValueVariant> value,
                               std::index_sequence<Is...> seq) noexcept
{
    (void) seq; // avoid compiler warning in release buiild
    //assert(span_is_valid<Ts...>(value, seq));
    if constexpr (sizeof...(Ts) == 1) {
        assert(std::get_if<Ts...>(&value[0]));
        return *std::get_if<Ts...>(&value[0]);
    }
    else {
        assert((... && std::get_if<Ts>(&value[Is])));
        return std::make_tuple(*std::get_if<Ts>(&value[Is])...);
    }
}

} // namespace internal::detail

/** @struct SocketAddr
 *  @brief Representation of a socket address.
 *
 *  IPv4 addresses have a nonzero port, as Skywing doesn't support
 *  using the 0 port, e.g., in bind().
 *
 *  @note This will waste 2 bytes in the "testing-only" Unix-socket
 *        scenario. I'm heartbroken.
 */
struct SocketAddr
{
    std::string m_addr;
    std::uint16_t m_port = 0;

    SocketAddr() noexcept = default;
    SocketAddr(std::string&& local_address) noexcept
        : m_addr{std::move(local_address)}, m_port{0}
    {}

    SocketAddr(std::string const& local_address)
        : m_addr{local_address}, m_port{0}
    {}

    SocketAddr(std::string&& addr, std::uint16_t port) noexcept
        : m_addr{std::move(addr)}, m_port{port}
    {}

    SocketAddr(std::string const& addr, std::uint16_t port)
        : m_addr{addr}, m_port{port}
    {}

    SocketAddr(SocketAddr&& addr_port_pair) noexcept = default;
    SocketAddr(SocketAddr const& addr_port_pair) = default;
    SocketAddr& operator=(SocketAddr&& addr_port_pair) noexcept = default;
    SocketAddr& operator=(SocketAddr const& addr_port_pair) = default;

    bool is_ipv4() const noexcept { return m_port; }
    bool is_unix() const noexcept { return !is_ipv4(); }

    std::string const& address() const noexcept { return m_addr; }
    std::uint16_t const& port() const noexcept { return m_port; }

    std::string str() const { return m_addr + ':' + std::to_string(m_port); }
}; // class SocketAddr

struct SockAddrCompare
{
    bool operator()(SocketAddr const& l, SocketAddr const& r) const noexcept
    {
        return (l.port() < r.port()) || (l.address() < r.address());
    }
}; // struct SockAddrCompare

inline bool operator<(SocketAddr const& a, SocketAddr const& b) noexcept
{
    return SockAddrCompare{}.operator()(a, b);
}

inline bool operator==(SocketAddr const& a, SocketAddr const& b) noexcept
{
    return (a.m_port == b.m_port) && (a.m_addr == b.m_addr);
}

inline bool operator!=(SocketAddr const& a, SocketAddr const& b) noexcept
{
    return !(a == b);
}

/** \brief Wrapper for returning void values in various situations
 */
struct VoidWrapper
{};

namespace internal
{
/// Wraps void values in VoidWrappers or returns the type unmodified
template <typename T>
using WrapVoidValue = std::conditional_t<
    // don't care about cv-qualified void because those shouldn't really be a
    // thing
    std::is_same_v<T, void>,
    VoidWrapper,
    T>;

/// Wraps void returning functions into returning VoidWrapper instead
template <typename Callable, typename... Args>
auto wrap_void_func(Callable&& c, Args&&... args) noexcept -> WrapVoidValue<
    decltype(::std::forward<Callable>(c)(::std::forward<Args>(args)...))>
{
    using RetType = WrapVoidValue<decltype(::std::forward<Callable>(c)(
        ::std::forward<Args>(args)...))>;
    if constexpr (std::is_same_v<RetType, VoidWrapper>) {
        ::std::forward<Callable>(c)(::std::forward<Args>(args)...);
        return VoidWrapper{};
    }
    else {
        return ::std::forward<Callable>(c)(::std::forward<Args>(args)...);
    }
}

/// Structure for testing if a value is any of the supplied values
template <typename T, T... Values>
struct any_of
{
    template <typename U>
    friend constexpr bool operator==(const U& comp, any_of) noexcept
    {
        return ((comp == Values) || ...);
    }

    template <typename U>
    friend constexpr bool operator==(any_of, const U& comp) noexcept
    {
        return comp == any_of{};
    }
};

} // namespace internal
} // namespace skywing

template <>
struct std::hash<skywing::SocketAddr>
{
    std::size_t operator()(const skywing::SocketAddr& val) const noexcept
    {
        return std::hash<std::string>{}(val.address())
               ^ std::hash<std::uint16_t>{}(val.port());
    }
};

#endif // SKYWING_TYPES_HPP
