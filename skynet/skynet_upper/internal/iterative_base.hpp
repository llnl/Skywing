#ifndef SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
#define SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP

#include "skynet_upper/pending_iterative.hpp"

namespace skynet::internal {
template<typename... TagValueTypes>
class IterativeBase {
public:
  /** \brief Rebuilds connections for dead tags
   */
  auto rebuild_dead_tags() noexcept -> Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>
  {
    auto to_ret = job_->rebuild_tags(dead_tags_);
    std::move(dead_tags_.begin(), dead_tags_.end(), std::back_inserter(tags_));
    dead_tags_.clear();
    return to_ret;
  }

  /** \brief Rebuilds the specified dead tags, ignoring any tags that aren't dead
   */
  template<typename Range>
  auto rebuild_dead_tags_range(const Range& r) noexcept -> Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>
  {
    std::vector<PublishTag<TagValueTypes...>> search_tags;
    for (const auto& tag : r) {
      auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) {
        search_tags.push_back(std::move(*iter));
        dead_tags_.erase(*iter);
      }
    }
    return job_->rebuild_tags(search_tags);
  }

  /** \brief Rebuilds the specified dead tags
   */
  template<typename... Tags>
  auto rebuild_dead_tags(const Tags&... tags) noexcept -> Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
    return rebuild_dead_tags_range(tag_array);
  }

  /** \brief Drops tracking for dead tags
   */
  void drop_dead_tags() noexcept
  {
    // TODO: Actually unsubscribe when that's a thing that can happen
    dead_tags_.clear();
  }

  /** \brief Drops tracking for specific tags, does nothing if the tags aren't dead
   */
  template<typename Range>
  void drop_dead_tags(const Range& r) noexcept
  {
    for (const auto& tag : r) {
      const auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) { dead_tags_.erase(iter); }
    }
  }

  /** \brief Drops the specified dead tags
   */
  template<typename... Tags>
  void drop_dead_tags(const Tags&... tags) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
  }

  /** \brief Returns all active tags
   */
  const std::vector<PublishTag<TagValueTypes...>>& tags() const noexcept { return tags_; }

  /** \brief Returns all dead tags
   */
  const std::vector<PublishTag<TagValueTypes...>>& dead_tags() const noexcept { return dead_tags_; }

protected:
  IterativeBase(
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const std::vector<PublishTag<TagValueTypes...>>& tags) noexcept
    : job_{&job}, produced_tag_{produced_tag}, tags_{tags}
  {
    for (auto tag_iter = tags_.begin(); tag_iter != tags_.end();) {
      if (!job_->tag_has_active_publisher(*tag_iter)) {
        dead_tags_.push_back(std::move(*tag_iter));
        tag_iter = tags_.erase(tag_iter);
      }
      else {
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
  MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const Range& tags) noexcept
{
  return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
    handle,
    job,
    produced_tag,
    gsl::make_span(tags) // gsl::make_span(tags.cbegin(), tags.cend())
  );
}

template<template<typename...> typename IterativeTemplate, typename... TagValueTypes, typename... TagTypes>
PendingIterativeMethod<IterativeTemplate<TagValueTypes...>> create_iterative(
  MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const TagTypes&... tags) noexcept
{
  const std::array<PublishTag<TagValueTypes...>, sizeof...(tags)> tags_array{tags...};
  return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
    handle, job, produced_tag, gsl::make_span(tags_array.cbegin(), tags_array.cend()));
}

template<
  template<typename...>
  typename IterativeTemplate,
  typename... TagValueTypes,
  typename Range,
  typename Rep,
  typename Period>
PendingIterativeMethod<IterativeTemplate<TagValueTypes...>> create_iterative(
  const std::chrono::time_point<Rep, Period>& end_time,
  IterativeInitErrorPolicy policy,
  MasterHandle handle,
  Job& job,
  const PublishTag<TagValueTypes...>& produced_tag,
  const Range& tags) noexcept
{
  return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
    handle,
    job,
    produced_tag,
    gsl::make_span(tags), // gsl::make_span(tags.cbegin(), tags.cend()),
    end_time,
    policy);
}

template<
  template<typename...>
  typename IterativeTemplate,
  typename... TagValueTypes,
  typename... TagTypes,
  typename Rep,
  typename Period>
PendingIterativeMethod<IterativeTemplate<TagValueTypes...>> create_iterative(
  const std::chrono::time_point<Rep, Period>& end_time,
  IterativeInitErrorPolicy policy,
  MasterHandle handle,
  Job& job,
  const PublishTag<TagValueTypes...>& produced_tag,
  TagTypes&&... tags) noexcept
{
  const std::array<PublishTag<TagValueTypes...>, sizeof...(tags)> tags_array{tags...};
  return PendingIterativeMethod<IterativeTemplate<TagValueTypes...>>::create(
    handle, job, produced_tag, gsl::make_span(tags_array.cbegin(), tags_array.cend()), end_time, policy);
}
} // namespace skynet::internal

#endif // SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
