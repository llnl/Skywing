#ifndef SKYNET_PUBSUB_CONVERTER_HPP
#define SKYNET_PUBSUB_CONVERTER_HPP

#include <tuple>
#include <utility>
#include <type_traits>
#include "skynet_core/types.hpp"
#include "skynet_mid/internal/iterative_helpers.hpp"

namespace skynet
{
  /** @brief Convert an object into a data type that Skynet can send
   * through its pubsub system.
   *
   *  This is a default struct template that works when T is a type
   *  that Skynet can send directly, the list of which can be found in
   *  \c skynet_core/types.hpp. You should implement a specialization
   *  of this struct template that converts your non-supported types
   *  into one of the supported types or into a std::tuple of
   *  supported types.
   *
   * @tparam T The type to convert into a supported type.
   */
  template<typename T>
  struct PubSubConverter
  {
    static_assert(true,
                  "You tried to apply the default PubSubConverter to a type that is not native to Skynet. You must implement a specialization of the PubSubConverter for your specific type.");
    
    using input_type = T;
    using pubsub_type = T;

    /** @brief Convert a T into something Skynet's pubsub handlers
     * natively support.
     *
     * In the default, T is already of that type, so just return the
     * input.
     */
    static pubsub_type convert(T t) { return t; }

    
    /** @brief Convert a Skynet pubsub type back into an original data type.
     *
     * In the default, T is already of that type, so just return the
     * input. Note that, in general, many data types can convert to
     * the same pubsub type; for example, a list of numbers and a
     * matrix might both convert to a \c
     * std::vector<double>. Therefore, we must know the original type
     * in order to correctly deconvert.
     */
    static input_type deconvert(pubsub_type ps_t) { return ps_t; }
  }; // struct PubSubConverter

  template<typename T>
  using PubSub_t = typename PubSubConverter<T>::pubsub_type;

  /** @brief PubSubConverter specialization for \c char* types.
   */
  template<>
  struct PubSubConverter<char*>
  {
    using pubsub_type = std::string;
    static pubsub_type convert(char* c_arr) { return std::string(c_arr); }
    static char* deconvert(std::string s) { return s.data(); }
  }; // struct PubSubConverter<char*>
} // namespace skynet

  
#include "skynet_mid/internal/pubsub_converter_helpers.hpp"

namespace skynet
{
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

    /** @brief Convert an input of type \c input_type into a tuple of type \c pusub_type.
     *
     * For example, suppose we want to send something of original type
     * \c std::tuple<T1, T2, T3>, and suppose 
     *
     * \code{.cpp}
     * PS1 = PubSubConverter<T1>::pubsub_type
     * std::tuple<PS21, PS22> = PubSubConverter<T2>::pubsub_type
     * PS3 = PubSubConverter<T3>::pubsub_type
     * \encode
     *
     * Then we have
     * \code{.cpp}
     * std::tuple<PS1, PS21, PS22, PS3> = PubSubConverter<std::tuple<T1, T2, T3>>::pubsub_type
     * \endcode
     * 
     * Now suppose we have a runtime \c my_tup of type \c std::tuple<T1, T2, T3>. Then we have
     * \code{.cpp}
     * using pubsub_t = typename PubSubConverter<std::tuple<T1, T2, T3>>::pubsub_type;
     * pubsub_t my_ps_tup = PubSubConverter<std::tuple<T1, T2, T3>>::convert(my_tup);
     * \encode
     */
    template<typename Indices = std::make_index_sequence<sizeof...(Ts)>>
    static pubsub_type convert(std::tuple<Ts...> tup)
    { 
      return convert_impl(std::move(tup), Indices{});
    }

    /** @brief Convert an input of type \c pubsub_type into something of type \c input_type.
     *
     * For example, suppose we are sending and receiving information of type
     * \c std::tuple<T1, T2, T3>, and suppose 
     *
     * \code{.cpp}
     * PS1 = PubSubConverter<T1>::pubsub_type
     * std::tuple<PS21, PS22> = PubSubConverter<T2>::pubsub_type
     * PS3 = PubSubConverter<T3>::pubsub_type
     * \encode
     *
     * Then we have
     * \code{.cpp}
     * std::tuple<PS1, PS21, PS22, PS3> = PubSubConverter<std::tuple<T1, T2, T3>>::pubsub_type
     * \endcode
     * 
     * Now suppose we have a runtime \c ps_tup of type \c
     * std::tuple<PS1, PS21, PS22, PS3> that we received from another
     * agent. Then we have
     * \code{.cpp}
     * using orig_type = typename PubSubConverter<std::tuple<T1, T2, T3>>::input_type; // is type std::tuple<T1, T2, T3>
     * orig_t my_tup = PubSubConverter<std::tuple<T1, T2, T3>>::deconvert(ps_tup);
     * \encode
     */
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

  // void test()
  // {
  //   double d = 2.0;
  //   unsigned u = 3;

  //   char colin_arr[] = "colin";
  //   std::string colin_s = PubSubConverter<char*>::convert(colin_arr);
  //   (void)colin_s;

  //   using MyTup = std::tuple<double, unsigned, int, std::tuple<double, char*>>;
  //   MyTup tup(d, u, 3, {1.2, colin_arr});
  //   //    d = tup;
  //   PubSub_t<MyTup> ps_tup = PubSubConverter<MyTup>::convert(tup);
  //   //    d = ps_tup;
  //   (void)ps_tup;

  //   MyTup t3 = PubSubConverter<MyTup>::deconvert(ps_tup);
  //   (void)t3;
  // }
}

#endif // SKYNET_PUBSUB_CONVERTER_HPP
