#ifndef SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/internal/iterative_base.hpp"
#include "skynet_upper/pending_iterative.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace skynet {

/** \brief Method for receiving information from a specified set of tags
 * in an asynchronous fashion
 */
template<typename... TagValueTypes>
class AsynchronousIterative : public internal::IterativeBase<TagValueTypes...> {
public:
  using ValueType = ValueOrTuple<TagValueTypes...>;

  AsynchronousIterative(Job& job,
                        const PublishTag<TagValueTypes...>& produced_tag,
                        const std::vector<PublishTag<TagValueTypes...>>& tags) noexcept
    : internal::IterativeBase<TagValueTypes...>{job, produced_tag, tags},
    values_(tags.size()),
    is_updated_(tags.size())
  {}

  /** \brief Returns values from all tags without submitting a value
   */
  auto values() noexcept -> std::tuple<const std::vector<ValueType>&,
                                       const std::vector<bool>&,
                                       const std::vector<PublishTag<TagValueTypes...>>&>
  {
    auto updated_iter = is_updated_.begin();
    auto value_iter = values_.begin();
    auto tag_iter = this->tags_.begin();
    const auto mark_current_as_dead = [&]() {
      this->dead_tags_.push_back(std::move(*tag_iter));
      value_iter = values_.erase(value_iter);
      updated_iter = is_updated_.erase(updated_iter);
      tag_iter = this->tags_.erase(tag_iter);
    };
    while (tag_iter != this->tags_.cend()) {
      const auto& tag = *tag_iter;
      if (!this->job_->tag_has_active_publisher(tag)) {
        mark_current_as_dead();
        continue;
      }
      else if (this->job_->has_data(tag)) {
        const auto value_opt = this->job_->get_waiter(tag).get();
        assert(value_opt);
        *updated_iter = true;
        *value_iter = *value_opt;
      }
      else {
        *updated_iter = false;
      }
      ++updated_iter;
      ++value_iter;
      ++tag_iter;
    }
    return {values_, is_updated_, this->tags_};
  }

  /** \brief Returns values from all tags while submitting a value
   */
  template<typename... ArgTypes>
  auto values(ArgTypes&&... values_to_submit) noexcept
    -> std::tuple<const std::vector<ValueType>&,
                  const std::vector<bool>&,
                  const std::vector<PublishTag<TagValueTypes...>>&>
  {
    submit_values(std::forward<ArgTypes>(values_to_submit)...);
    return values();
  }

  /** \brief Submit a value
   */
  template<typename... ArgTypes>
  auto submit_values(ArgTypes&&... values_to_submit) noexcept
  {
    this->job_->publish(this->produced_tag_, std::forward<ArgTypes>(values_to_submit)...);
  }

private:
  template<typename, typename>
  friend class PendingIterativeMethod;

  std::vector<ValueType> values_;
  std::vector<bool> is_updated_;
}; // class AsynchronousIterative

template<typename... TagValueTypes, typename Range>
auto create_asynchronous_iterative(
  MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const Range& tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
    job.subscribe_range(tags),
    handle,
    job,
    produced_tag,
    tags
  );
}

template<typename... TagValueTypes, typename... TagTypes>
auto create_asynchronous_iterative(
  MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const TagTypes&... tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
    job.subscribe(tags...),
    handle,
    job,
    produced_tag,
    tags...
  );
}

template<
  typename... TagValueTypes,
  typename Range,
  typename Rep,
  typename Period>
auto create_asynchronous_iterative(
  const std::chrono::time_point<Rep, Period>& end_time,
  IterativeInitErrorPolicy policy,
  MasterHandle handle,
  Job& job,
  const PublishTag<TagValueTypes...>& produced_tag,
  const Range& tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
    job.subscribe_range(tags),
    end_time,
    policy,
    handle,
    job,
    produced_tag,
    tags
  );
}

template<
  typename... TagValueTypes,
  typename... TagTypes,
  typename Rep,
  typename Period>
auto create_asynchronous_iterative(
  const std::chrono::time_point<Rep, Period>& end_time,
  IterativeInitErrorPolicy policy,
  MasterHandle handle,
  Job& job,
  const PublishTag<TagValueTypes...>& produced_tag,
  const TagTypes&... tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
    job.subscribe(tags...),
    end_time,
    policy,
    handle,
    job,
    produced_tag,
    tags...
  );
}
} // namespace skynet

#endif // SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
