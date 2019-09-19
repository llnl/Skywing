#ifndef SKYNET_INTERNAL_UTILITY_TYPE_LIST_HPP
#define SKYNET_INTERNAL_UTILITY_TYPE_LIST_HPP

namespace skynet { namespace internal
{
  /** \brief Structure for representing a list of types
   */
  template<typename... T>
  struct TypeList{};

  namespace detail
  {
    // Base class for specialization
    template<typename SearchFor, typename... T>
    struct IndexOfImpl;

    // No match, just have it be zero so the size of the list is returned
    template<typename SearchFor>
    struct IndexOfImpl<SearchFor, TypeList<>>
    {
      static constexpr int value = 0;
    };

    // Match
    template<typename SearchFor, typename... T>
    struct IndexOfImpl<SearchFor, TypeList<SearchFor, T...>>
    {
      static constexpr int value = 0;
    };

    // No match
    template<typename SearchFor, typename First, typename... Rest>
    struct IndexOfImpl<SearchFor, TypeList<First, Rest...>>
    {
      static constexpr int value = 1 + IndexOfImpl<SearchFor, TypeList<Rest...>>::value;
    };
  } // namespace detail

  /** \brief The index of a type in a TypeList, or the size of the list if it is not present
   */
  template<typename SearchFor, typename List>
  constexpr int index_of = detail::IndexOfImpl<SearchFor, List>::value;

  namespace detail
  {
    template<typename... T>
    struct SizeImpl;

    template<typename... T>
    struct SizeImpl<TypeList<T...>>
    {
      static constexpr int value = sizeof...(T);
    };
  } // namespace detail

  /** \brief The size of a type list
   */
  template<typename List>
  constexpr int size = detail::SizeImpl<List>::value;

  namespace detail
  {
    template<int Index, typename... T>
    struct AtImpl;

    template<typename First, typename... Rest>
    struct AtImpl<0, TypeList<First, Rest...>>
    {
      using Type = First;
    };

    template<int Index, typename First, typename... Rest>
    struct AtImpl<Index, TypeList<First, Rest...>>
    {
      using Type = typename AtImpl<Index - 1, TypeList<Rest...>>::Type;
    };
  } // namespace detail

  /** \brief The type at the specified index
   */
  template<int Index, typename List>
  using At = typename detail::AtImpl<Index, List>::Type;
} } // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_TYPE_LIST_HPP
