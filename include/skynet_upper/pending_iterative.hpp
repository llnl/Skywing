#ifndef SKYNET_UPPER_INTERNAL_PENDING_ITERATIVE_HPP
#define SKYNET_UPPER_INTERNAL_PENDING_ITERATIVE_HPP

#include "skynet_core/master.hpp"
#include "skynet_core/job.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <type_traits>

namespace skynet
{
  template<typename IterativeMethod>
  class PendingIterativeMethod;

  /** \brief Return type for creating synchronous iterative classes
   */
  template<template<typename...> typename IterativeMethod, typename... TagValueTypes>
  class PendingIterativeMethod<IterativeMethod<TagValueTypes...>>
  {
  private:
    struct IsReady;
    struct GetIterativeOpt;

  public:
    PendingIterativeMethod(const PendingIterativeMethod&) = delete;
    PendingIterativeMethod& operator=(const PendingIterativeMethod&) = delete;

    auto get_waiter() & noexcept
      -> Continuation<Waiter<IsReady, WaiterGetNoOp>, GetIterativeOpt>&
    {
      return is_ready_waiter_();
    }

    std::optional<IterativeMethod<TagValueTypes...>> get() noexcept
    {
      return is_ready_waiter_.get();
    }

    /** \brief Create a pending IterativeMethod class - generally not intended to be
     * called directly, create_synchronous_iterative should be called instead.
     */
    static PendingIterativeMethod create(
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
      return PendingIterativeMethod{handle, job, produced_tag, tags};
    }

  private:
    struct IsReady
    {
      PendingIterativeMethod* to_ret;
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
      PendingIterativeMethod* to_ret;
      std::optional<IterativeMethod<TagValueTypes...>> operator()() const noexcept
      {
        if (to_ret->error_occurred_) { return std::nullopt; }
        return IterativeMethod<TagValueTypes...>{*to_ret->job_, to_ret->produced_tag_, to_ret->tags_};
      }
    }; // struct GetIterativeOpt

    PendingIterativeMethod(
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
  }; // class PendingIterativeMethod
} // namespace skynet

#endif // SKYNET_UPPER_INTERNAL_PENDING_ITERATIVE_HPP
