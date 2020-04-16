#ifndef SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
#define SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP

#include "skynet_upper/pending_iterative.hpp"

namespace skynet::internal
{
  template<typename... TagValueTypes>
  class IterativeBase
  {
  public:
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

  protected:
    IterativeBase(
      Job& job,
      const PublishTag<TagValueTypes...>& produced_tag,
      const std::vector<PublishTag<TagValueTypes...>>& tags
    ) noexcept
      : job_{&job}
      , produced_tag_{produced_tag}
      , tags_{tags}
    {
      for (auto tag_iter = tags_.begin(); tag_iter != tags_.end();)
      {
        if (!job_->tag_has_active_publisher(*tag_iter))
        {
          dead_tags_.push_back(std::move(*tag_iter));
          tag_iter = tags_.erase(tag_iter);
        }
        else
        {
          ++tag_iter;
        }
      }
    }

    Job* job_;
    PublishTag<TagValueTypes...> produced_tag_;
    std::vector<PublishTag<TagValueTypes...>> tags_;
    std::vector<PublishTag<TagValueTypes...>> dead_tags_;
  }; // class IterativeBase

  template<template<typename...> typename IterativeTemplate, typename... TagValueTypes, typename Range>
  PendingIterativeMethod<IterativeTemplate<TagValueTypes...>> create_iterative(
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const Range& tags
  ) noexcept
  {
    return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags.cbegin(), tags.cend())
    );
  }

  template<template<typename...> typename IterativeTemplate, typename... TagValueTypes, typename... TagTypes>
  PendingIterativeMethod<IterativeTemplate<TagValueTypes...>> create_iterative(
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const TagTypes&... tags
  ) noexcept
  {
    const std::array<PublishTag<TagValueTypes...>, sizeof...(tags)> tags_array{tags...};
    return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags_array.cbegin(), tags_array.cend())
    );
  }

  template<
    template<typename...> typename IterativeTemplate,
    typename... TagValueTypes,
    typename Range,
    typename Rep,
    typename Period
  >
  PendingIterativeMethod<IterativeTemplate<TagValueTypes...>> create_iterative(
    const std::chrono::time_point<Rep, Period>& end_time,
    IterativeInitErrorPolicy policy,
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const Range& tags
  ) noexcept
  {
    return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags.cbegin(), tags.cend()),
      end_time,
      policy
    );
  }

  template<
    template<typename...> typename IterativeTemplate,
    typename... TagValueTypes,
    typename... TagTypes,
    typename Rep,
    typename Period
  >
  PendingIterativeMethod<IterativeTemplate<TagValueTypes...>> create_iterative(
    const std::chrono::time_point<Rep, Period>& end_time,
    IterativeInitErrorPolicy policy,
    MasterHandle handle,
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    TagTypes&&... tags
  ) noexcept
  {
    const std::array<PublishTag<TagValueTypes...>, sizeof...(tags)> tags_array{tags...};
    return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
      handle,
      job,
      produced_tag,
      gsl::make_span(tags_array.cbegin(), tags_array.cend()),
      end_time,
      policy
    );
  }
} // namespace skynet::internal

#endif // SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
