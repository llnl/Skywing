#ifndef SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/master.hpp"
#include "skynet_core/job.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <type_traits>
#include <utility>

namespace skynet
{
  template<typename... TagValueTypes>
  class PendingSynchronousIterative;

  /** \brief Method for receiving information from a specified set of tags
   * in a synchronous fashion
   */
  template<typename... TagValueTypes>
  class SynchronousIterative
  {
  public:
    using ValueType = ValueOrTuple<TagValueTypes...>;

    /** \brief Retrieves the values from the other tags, not being ready
     * until all tags have either received values or errored.
     *
     * If a subscription for a tag become unavailable, the waiter will
     * return an empty vector.
     */
    auto values() noexcept
    {
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
          std::vector<ValueType> values;
          values.reserve(opt_values.size());
          auto tag_iter = tags_.begin();
          for (const auto& val : opt_values)
          {
            if (val)
            {
              if (!errored)
              {
                values.push_back(*val);
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
          return errored ? std::vector<ValueType>{} : values;
        });
    }

    /** \brief Retrieves the values from other tags, not being ready
     * until all tags have either received values or errored.
     *
     * The waiter return type is
     * std::pair<std::vector<ValueType>, const std::vector<PublishTag<TagValueTypes...>&>
     * The vector of tags indicate which tag each value is from.
     */
    auto values_ignore_errors() noexcept
    {
      using RetType = std::pair<
        std::vector<ValueType>,
        const std::vector<PublishTag<TagValueTypes...>>&
      >;
      using WaiterType = decltype(job_->get_waiter(tags_.front()));
      std::vector<WaiterType> waiters;
      waiters.reserve(tags_.size());
      return when_all_same(waiters)
        .then([this](const std::vector<std::optional<ValueType>>& opt_values) noexcept -> RetType {
          std::vector<ValueType> values;
          values.reserve(opt_values.size());
          auto tag_iter = tags_.begin();
          for (const auto& val : opt_values)
          {
            if (val)
            {
              values.push_back(*val);
              ++tag_iter;
            }
            else
            {
              dead_tags_.push_back(std::move(*tag_iter));
              tag_iter = tags_.erase(tag_iter);
            }
          }
          return {values, tags_};
        });
    }

    /** \brief Submit a value to neighbors
     */
    template<typename... ArgTypes>
    void submit_values(ArgTypes&&... values) noexcept
    {
      job_->publish(produced_tag_, std::forward<ArgTypes>(values)...);
    }
    void submit_values(const std::tuple<TagValueTypes...>& values) noexcept
    {
      job_->publish(produced_tag_, values);
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
    friend class PendingSynchronousIterative<TagValueTypes...>;

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

  /** \brief Return type for creating synchronous iterative classes
   */
  template<typename... TagValueTypes>
  class PendingSynchronousIterative
  {
  private:
    struct IsReady;
    struct GetIterativeOpt;

  public:
    PendingSynchronousIterative(const PendingSynchronousIterative&) = delete;
    PendingSynchronousIterative& operator=(const PendingSynchronousIterative&) = delete;

    auto get_waiter() & noexcept
      -> Continuation<Waiter<IsReady, WaiterGetNoOp>, GetIterativeOpt>&
    {
      return is_ready_waiter_();
    }

    std::optional<SynchronousIterative<TagValueTypes...>> get() noexcept
    {
      return is_ready_waiter_.get();
    }

    /** \brief Create a pending SynchronousIterative class - generally not intended to be
     * called directly, create_synchronous_iterative should be called instead.
     */
    static PendingSynchronousIterative create(
      MasterHandle handle,
      Job& job,
      const PublishTag<TagValueTypes...>& produced_tag,
      const gsl::span<const PublishTag<TagValueTypes...>> tags
    ) noexcept
    {
      const auto iter = std::find(tags.cbegin(), tags.cend(), produced_tag);
      if (iter == tags.cend())
      {
        std::cerr << "Produced tag for SynchronousIterative not found in the tag list!\n";
        std::exit(2);
      }
      job.declare_publication_intent(produced_tag);
      return PendingSynchronousIterative{handle, job, produced_tag, tags};
    }

  private:
    struct IsReady
    {
      PendingSynchronousIterative* to_ret;
      bool operator()() const noexcept
      {
        if (!to_ret->subscribe_done_)
        {
          if (!to_ret->subscribe_waiter_.is_ready())
          {
            return false;
          }
          to_ret->subscribe_done_ = true;
        }
        if (!to_ret->job_->tags_have_subscriptions(to_ret->tags_))
        {
          to_ret->error_occurred_ = true;
          return true;
        }
        return to_ret->job_->number_of_subscribers(to_ret->produced_tag_) >= static_cast<int>(to_ret->tags_.size());
      }
    }; // struct IsReady

    struct GetIterativeOpt
    {
      PendingSynchronousIterative* to_ret;
      std::optional<SynchronousIterative<TagValueTypes...>> operator()() const noexcept
      {
        if (to_ret->error_occurred_) { return std::nullopt; }
        return SynchronousIterative<TagValueTypes...>{*to_ret->job_, to_ret->produced_tag_, to_ret->tags_};
      }
    }; // struct GetIterativeOpt

    PendingSynchronousIterative(
      MasterHandle handle,
      Job& job,
      const PublishTag<TagValueTypes...>& produced_tag,
      const gsl::span<const PublishTag<TagValueTypes...>> tags
    ) noexcept
      : subscribe_waiter_{job.subscribe_range(tags)}
      , is_ready_waiter_{handle.waiter_on_subscription_change(IsReady{this}).then(GetIterativeOpt{this})}
      , tags_(tags.cbegin(), tags.cend())
      , produced_tag_{produced_tag}
      , job_{&job}
    {}

    bool error_occurred_ = false;
    bool subscribe_done_ = false;
    Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp> subscribe_waiter_;
    Continuation<Waiter<IsReady, WaiterGetNoOp>, GetIterativeOpt> is_ready_waiter_;
    std::vector<PublishTag<TagValueTypes...>> tags_;
    PublishTag<TagValueTypes...> produced_tag_;
    Job* job_;
  }; // class PendingSynchronousIterative

  template<typename... TagValueTypes, typename Range>
  PendingSynchronousIterative<TagValueTypes...> create_synchronous_iterative(
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const Range& tags
  ) noexcept
  {
    return PendingSynchronousIterative<TagValueTypes...>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags.cbegin(), tags.cend())
    );
  }

  template<typename... TagValueTypes, typename... TagTypes>
  PendingSynchronousIterative<TagValueTypes...> create_synchronous_iterative(
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    TagTypes... tags
  ) noexcept
  {
    const std::array<PublishTag<TagValueTypes...>, sizeof...(tags)> tags_array{tags...};
    return PendingSynchronousIterative<TagValueTypes...>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags_array.cbegin(), tags_array.cend())
    );
  }
} // namespace skynet

#endif // SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
