#ifndef SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/job.hpp"

#include <optional>
#include <type_traits>
#include <utility>

namespace skynet
{
  /** \brief Method for recieving information from a specified set of tags
   *
   * TODO: Put this in a create function / future interface thing instead
   * of having creation of them block
   */
  template<typename ValueType, typename... TagTypes>
  class SynchronousIterative
  {
  private:
    using TupleType = std::tuple<typename TagTypes::ValueType...>;
    using ValueRetType = std::optional<ValueOrTuple<typename TagTypes::ValueType...>>;

  public:
    /** \brief Create an object for synchronously iterating over pairs of
     * subscriptions
     *
     * TODO: Does it make sense to have everything pass all of the tags used
     * while also specifying its own tag?  This is similar to how the reduce
     * groups work, and is probably more intuitive for the user / easier to
     * ensure that everything is using the same tags across each machine.
     * It will take quite a bit more work to implement, however, so leave it
     * as-is for now (which is quite easy to implement)
     */
    explicit SynchronousIterative(
      Job& job,
      const PublishTag<ValueType>& produced_tag,
      const ValueType& initial_value,
      const TagTypes&... tags
    ) noexcept
      : job_{job}
      , produced_tag_{produced_tag}
      , tags_{tags...}
    {
      job.declare_publication_intent(produced_tag);
      job.subscribe(tags...).get();
      // TODO: Turn this into a future (waiting for all subscribers)
      while (job.num_subscriptions(produced_tag) != sizeof...(TagTypes))
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      submit_value(initial_value);
    }

    /** \brief Retrieves the values from the other tags, blocking until it
     * is available.
     *
     * If a subscription for a tag become unavailable, this function will
     * return an empty optional.
     */
    ValueRetType values() noexcept
    {
      TupleType to_ret;
      if (tuple_setter(to_ret, std::index_sequence_for<TagTypes...>{}))
      {
        if constexpr (sizeof...(TagTypes) == 0)
        {
          return std::get<0>(to_ret);
        }
        else
        {
          return to_ret;
        }
      }
      return std::nullopt;
    }

    /** \brief Submit a value to neighbors
     */
    void submit_value(const ValueType& value) noexcept
    {
      job_.publish(produced_tag_, value);
    }

  private:
    /** \brief Sets a value in the return tuple from the tags.
     * Returns true if the value could be retrieved, returns false
     * if it could not.
     */
    template<std::size_t Index>
    bool tuple_setter_single(TupleType& to_set)
    {
      const auto value = job_.get_waiter(std::get<Index>(tags_)).get();
      if (!value)
      {
        return false;
      }
      std::get<Index>(to_set) = *value;
      return true;
    }

    /** \brief Loads all values into a tuple, returning true if the values
     * could be loaded and false otherwise
     */
    template<std::size_t... Indexes>
    bool tuple_setter(TupleType& to_set, std::index_sequence<Indexes...>)
    {
      return (... && tuple_setter_single<Indexes>(to_set));
    }

    // Waiter<void, internal::MasterSubscribeIsDone, internal::WaiterGetNoOp> subscribe_future_;
    Job& job_;
    PublishTag<ValueType> produced_tag_;
    std::tuple<TagTypes...> tags_;
  }; // class SynchronousIterative
} // namespace skynet

#endif // SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
