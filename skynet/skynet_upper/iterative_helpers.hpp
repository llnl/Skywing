#ifndef SKYNET_ITERATIVE_HELPERS_HPP
#define SKYNET_ITERATIVE_HELPERS_HPP

#include <tuple>
#include <type_traits>

namespace skynet
{
  /** @brief struct that wraps a type in a tuple if it isn't already a tuple.
   */
  template<typename S>
  struct TupleIfNotAlready
  {
    using tuple_type = std::tuple<S>;
    static constexpr std::size_t tuple_size = 1;
    static tuple_type to_tuple(S t) { return std::tuple<S>(t); }
  };
  template<typename... Ss>
  struct TupleIfNotAlready<std::tuple<Ss...>>
  {
    using tuple_type = std::tuple<Ss...>;
    static constexpr std::size_t tuple_size = std::tuple_size_v<tuple_type>;
    static tuple_type to_tuple(tuple_type t) { return t; }
  };


  /** @brief Get the ValueType typename in a type, wrapped in a tuple
      if not already, or an empty tuple if it doesn't exist.
   */
  template<typename, typename = void>
  struct ValueTypeAsTuple
  {
    using type = std::tuple<>;
  };
  
  template<typename T>
  struct ValueTypeAsTuple<T, std::void_t<typename T::ValueType>>
  {
    using type = typename TupleIfNotAlready<typename T::ValueType>::tuple_type;
  };

  /** @brief Get std::tuple<T1::ValueType, T2::ValueType, ...> if all ValueTypes are tuples.
   */
  template<typename... Ts>
  struct TupleOfValueTypes
  {
    using type = decltype(std::tuple_cat(std::declval<typename ValueTypeAsTuple<Ts>::type>()...));
  };

  template<typename... Ts>
  using TupleOfValueTypes_t = typename TupleOfValueTypes<Ts...>::type;

  // void testblah()
  // {
  //   struct T1 { using ValueType = std::tuple<int>; };
  //   struct T2 { using ValueType = double; };
  //   struct T3 { };
  //   typename ValueTypesInTuple<T1, T3, T2, T3>::type v = "hello";
  //   (void)v;
  // }
    
  
} // namespace skynet

#endif // SKYNET_ITERATIVE_HELPERS_HPP
