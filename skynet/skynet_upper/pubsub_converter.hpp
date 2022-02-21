#ifndef SKYNET_PUBSUB_CONVERTER_HPP
#define SKYNET_PUBSUB_CONVERTER_HPP

#include <tuple>
#include <utility>
#include <type_traits>
#include "skynet_upper/iterative_helpers.hpp"

namespace skynet
{
  /** @brief Convert an object into a data type that Skynet can send through its pubsub system.
   *
   *  This is a default struct template that works when T is itself a
   *  Plain Old Data type that Skynet can send directly. You should
   *  implement a specialization of this struct template for your
   *  non-POD type.
   *
   * @tparam T The type to convert into Plain Old Data.
   */
  template<typename T>
  struct PubSubConverter
  {
    using input_type = T;
    using pubsub_type = T;

    static pubsub_type convert(T t) { return t; }
    static input_type deconvert(pubsub_type ps_t) { return ps_t; }
  }; // struct PubSubConverter

  template<typename T>
  using PubSub_t = typename PubSubConverter<T>::pubsub_type;

  template<>
  struct PubSubConverter<char*>
  {
    using pubsub_type = std::string;
    static pubsub_type convert(char* c_arr) { return std::string(c_arr); }
    static char* deconvert(std::string s) { return s.data(); }
  }; // struct PubSubConverter<char*>







  template<typename S>
  auto convert_and_tuplify(S t)
  {
    return TupleIfNotAlready<PubSub_t<S>>::to_tuple(PubSubConverter<S>::convert(t));
  }


  /* @brief Get the number of tuple elements, after conversion, of I'th element of Ts.
   */
  template<std::size_t I, typename... Ts>
  struct size_of_converted
  {
    using orig_type = std::tuple_element_t<I, std::tuple<Ts...>>;
    using PSType = PubSub_t<orig_type>;
    static constexpr std::size_t size = std::tuple_size_v<typename TupleIfNotAlready<PSType>::tuple_type>;
  };
  template<std::size_t I, typename... Ts>
  inline constexpr std::size_t size_of_converted_v = size_of_converted<I, Ts...>::size;

  
  /* @brief Get the number of tuple elements, after conversion, of first N Ts
   */
  template<int N, typename... Ts>
  struct size_of_first_N
  {
    static constexpr std::size_t size = size_of_converted_v<N, Ts...> + size_of_first_N<N-1, Ts...>::size;
  };
  template<typename... Ts>
  struct size_of_first_N<-1, Ts...>
  {
    static constexpr std::size_t size = 0;
  };
  template<int N, typename... Ts>
  inline constexpr std::size_t size_of_first_N_v = size_of_first_N<N, Ts...>::size;


  /* @brief Constructs an std::index_sequence<N, N+1, ..., N+k> 
   */
  template<std::size_t N, typename Seq> struct offset_sequence;
  
  template<std::size_t N, std::size_t... Ints>
  struct offset_sequence<N, std::index_sequence<Ints...>>
  {
    using type = std::index_sequence<Ints + N...>;
  };
  template<std::size_t N, typename Seq>
  using offset_sequence_t = typename offset_sequence<N, Seq>::type;
  

  /* @brief Create a sub-tuple out of a subset of its indices.
   */
  template<typename Tuple, std::size_t... Ints>
  auto select_tuple(Tuple tuple, std::index_sequence<Ints...>)
  {
    return std::tuple<std::tuple_element_t<Ints, Tuple>...>(std::get<Ints>(std::forward<Tuple>(tuple))...);
  }

  /* @brief Get the sub-tuple associated with the Nth element of the pubsub_type tuple.
   */
  template<int I, typename PSTuple, typename... Ts>
  auto extract_Nth_tuple(PSTuple ps_tup)
  {
    constexpr std::size_t Nth_size = size_of_converted_v<I, Ts...>;
    constexpr std::size_t num_preceding = I == 0 ? 0 : size_of_first_N_v<I-1, Ts...>;
    using index_seq_for_Nth = offset_sequence_t<num_preceding, std::make_index_sequence<Nth_size>>;

    return select_tuple(ps_tup, index_seq_for_Nth{});
  }

  /* @brief If the tuple has one element, return it, otherwise return the tuple.
   */
  template<typename Tuple>
  auto remove_tuple_if_single(Tuple tup)
  {
    if constexpr (std::tuple_size_v<Tuple> == 1) return std::get<0>(tup);
    else return tup;
  }



  /** @brief PubSubConverter specialization for tuple types.
   *
   * Recursively applies a PubSubConverter to each type in the tuple
   * and then *flattens* nested tuples into a single-level tuple of
   * converted elements.
   *
   * E.g.
   * <tt>
   * using MyTuple = std::tuple<double, unsigned, int, std::tuple<double, char*>>;
   * using MyPSTuple = PubSub_t<MyTuple>; // has type std::tuple<double, unsigned, int, double, std::string>
   * </tt>
   */
  template<typename... Ts>
  struct PubSubConverter<std::tuple<Ts...>>
  {
    using input_type = std::tuple<Ts...>;
    using pubsub_type =
      decltype(std::tuple_cat(std::declval<typename TupleIfNotAlready<PubSub_t<Ts>>::tuple_type>()...));

    template<typename Indices = std::make_index_sequence<sizeof...(Ts)>>
    static pubsub_type convert(std::tuple<Ts...> tup)
    { 
      return convert_impl(std::move(tup), Indices{});
    }

    template<typename Indices = std::make_index_sequence<sizeof...(Ts)>>
    static input_type deconvert(pubsub_type ps_tup)
    { 
      return deconvert_impl(std::move(ps_tup), Indices{});
    }

  private:
    template<std::size_t... I>
    static pubsub_type convert_impl(std::tuple<Ts...> tup, std::index_sequence<I...>)
    {
      return std::tuple_cat(convert_and_tuplify(std::move(std::get<I>(tup)))...);
    }



    template<std::size_t I>
    static auto extract_and_deconvert(pubsub_type& tup)
    {
      using T = std::tuple_element_t<I, std::tuple<Ts...>>;
      return PubSubConverter<T>::deconvert
        (remove_tuple_if_single(extract_Nth_tuple<I, pubsub_type, Ts...>(tup)));
    }

    template<std::size_t... I>
    static input_type deconvert_impl(pubsub_type tup, std::index_sequence<I...>)
    {
      return std::make_tuple(extract_and_deconvert<I>(tup)...);
    }
  }; // struct PubSubConverter<std::tuple<Ts...>>

  void test()
  {
    double d = 2.0;
    unsigned u = 3;

    char colin_arr[] = "colin";
    std::string colin_s = PubSubConverter<char*>::convert(colin_arr);
    (void)colin_s;

    using MyTup = std::tuple<double, unsigned, int, std::tuple<double, char*>>;
    MyTup tup(d, u, 3, {1.2, colin_arr});
    //    d = tup;
    PubSub_t<MyTup> ps_tup = PubSubConverter<MyTup>::convert(tup);
    //    d = ps_tup;
    (void)ps_tup;

    MyTup t3 = PubSubConverter<MyTup>::deconvert(ps_tup);
    (void)t3;
  }
}

#endif // SKYNET_PUBSUB_CONVERTER_HPP
