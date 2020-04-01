#ifndef SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/pending_iterative.hpp"

#include <utility>

namespace skynet
{
  /** \brief Method for receiving information from a specified set of tags
   * in a synchronous fashion
   */
  template<typename... TagValueTypes>
  class SynchronousIterative
  {
  public:
    using ValueType = ValueOrTuple<TagValueTypes...>;

    /** \brief Retrieves the values from all tags, not being ready
     * until all tags have either received values or errored.
     *
     * If a subscription for a tag become unavailable, the waiter will
     * return an empty vector.
     */
    template<typename... ArgTypes>
    auto values(ArgTypes&&... submit_values) noexcept
    {
      job_->publish(produced_tag_, std::forward<ArgTypes>(submit_values)...);
      using WaiterType = decltype(job_->get_waiter(tags_.front()));
      std::vector<WaiterType> waiters;
      waiters.reserve(tags_.size());
      for (const auto& tag : tags_)
      {
        waiters.push_back(job_->get_waiter(tag));
      }
      return when_all_same(waiters)
        .then([&](const std::vector<std::optional<ValueType>>& opt_values) noexcept -> std::vector<ValueType> {
          bool errored = false;
          std::vector<ValueType> ret_values;
          ret_values.reserve(opt_values.size());
          auto tag_iter = tags_.begin();
          for (const auto& val : opt_values)
          {
            if (val)
            {
              if (!errored)
              {
                ret_values.push_back(*val);
              }
              ++tag_iter;
            }
            else
            {
              errored = true;
              dead_tags_.push_back(std::move(*tag_iter));
              tag_iter = tags_.erase(tag_iter);
            }
          }
          return errored ? std::vector<ValueType>{} : ret_values;
        });
    }

    /** \brief Retrieves the values from all tags, not being ready
     * until all tags have either received values or errored.
     *
     * The waiter return type is
     * std::pair<std::vector<ValueType>, const std::vector<PublishTag<TagValueTypes...>&>
     * The vector of tags indicate which tag each value is from.
     */
    template<typename... ArgTypes>
    auto values_ignore_errors(ArgTypes&&... submit_values) noexcept
    {
      job_->publish(produced_tag_, std::forward<ArgTypes>(submit_values)...);
      using RetType = std::pair<
        std::vector<ValueType>,
        const std::vector<PublishTag<TagValueTypes...>>&
      >;
      using WaiterType = decltype(job_->get_waiter(tags_.front()));
      std::vector<WaiterType> waiters;
      waiters.reserve(tags_.size());
      return when_all_same(waiters)
        .then([this](const std::vector<std::optional<ValueType>>& opt_values) noexcept -> RetType {
          std::vector<ValueType> ret_values;
          ret_values.reserve(opt_values.size());
          auto tag_iter = tags_.begin();
          for (const auto& val : opt_values)
          {
            if (val)
            {
              ret_values.push_back(*val);
              ++tag_iter;
            }
            else
            {
              dead_tags_.push_back(std::move(*tag_iter));
              tag_iter = tags_.erase(tag_iter);
            }
          }
          return {ret_values, tags_};
        });
    }

    /** \brief Rebuilds connections for dead tags
     */
    auto rebuild_dead_tags()
      -> Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>
    {
      auto to_ret = job_->rebuild_tags(dead_tags_);
      std::move(
        dead_tags_.begin(),
        dead_tags_.end(),
        std::back_inserter(tags_)
      );
      dead_tags_.clear();
      return to_ret;
    }

    /** \brief Drops tracking for dead tags
     */
    void drop_dead_tags()
    {
      // TODO: Actually unsubscribe when that's a thing that can happen
      dead_tags_.clear();
    }

  private:
    friend class PendingIterativeMethod<SynchronousIterative>;

    SynchronousIterative(
      Job& job,
      const PublishTag<TagValueTypes...>& produced_tag,
      const std::vector<PublishTag<TagValueTypes...>>& tags
    ) noexcept
      : job_{&job}
      , produced_tag_{produced_tag}
      , tags_{tags}
    {}

    Job* job_;
    PublishTag<TagValueTypes...> produced_tag_;
    std::vector<PublishTag<TagValueTypes...>> tags_;
    std::vector<PublishTag<TagValueTypes...>> dead_tags_;
  }; // class SynchronousIterative

  template<typename... TagValueTypes, typename Range>
  PendingIterativeMethod<SynchronousIterative<TagValueTypes...>> create_synchronous_iterative(
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const Range& tags
  ) noexcept
  {
    return PendingIterativeMethod<SynchronousIterative<TagValueTypes...>>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags.cbegin(), tags.cend())
    );
  }

  template<typename... TagValueTypes, typename... TagTypes>
  PendingIterativeMethod<SynchronousIterative<TagValueTypes...>> create_synchronous_iterative(
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    TagTypes... tags
  ) noexcept
  {
    const std::array<PublishTag<TagValueTypes...>, sizeof...(tags)> tags_array{tags...};
    return PendingIterativeMethod<SynchronousIterative<TagValueTypes...>>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags_array.cbegin(), tags_array.cend())
    );
  }
} // namespace skynet

#endif // SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
