#ifndef SKYWING_INTERNAL_UTILITY_OVERLOAD_SET_HPP
#define SKYWING_INTERNAL_UTILITY_OVERLOAD_SET_HPP

#include <utility>

namespace skywing::internal
{
/** \brief A struct to create and overload set from unrelated function objects
 */
template <typename... Bases>
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
template <typename... Ts>
OverloadSet<Ts...> make_overload_set(Ts&&... callables) noexcept
{
    return OverloadSet<Ts...>{std::forward<Ts>(callables)...};
}
} // namespace skywing::internal

#endif // SKYWING_INTERNAL_UTILITY_OVERLOAD_SET_HPP
