#ifndef SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/internal/iterative_base.hpp"
#include "skynet_upper/pending_iterative.hpp"

#include <utility>

namespace skynet {
/** \brief Method for receiving information from a specified set of tags
 * in a synchronous fashion
 */
template<typename... TagValueTypes>
class SynchronousIterative : public internal::IterativeBase<TagValueTypes...> {
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
    this->job_->publish(this->produced_tag_, std::forward<ArgTypes>(submit_values)...);
    using WaiterType = decltype(this->job_->get_waiter(this->tags_.front()));
    std::vector<WaiterType> waiters;
    waiters.reserve(this->tags_.size());
    for (const auto& tag : this->tags_) {
      waiters.push_back(this->job_->get_waiter(tag));
    }
    return when_all_same(waiters).then(
      [&](const std::vector<std::optional<ValueType>>& opt_values) noexcept -> std::vector<ValueType> {
        bool errored = false;
        std::vector<ValueType> ret_values;
        ret_values.reserve(opt_values.size());
        auto tag_iter = this->tags_.begin();
        for (const auto& val : opt_values) {
          if (val) {
            if (!errored) { ret_values.push_back(*val); }
            ++tag_iter;
          }
          else {
            errored = true;
            this->dead_tags_.push_back(std::move(*tag_iter));
            tag_iter = this->tags_.erase(tag_iter);
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
    this->job_->publish(this->produced_tag_, std::forward<ArgTypes>(submit_values)...);
    using RetType = std::pair<std::vector<ValueType>, const std::vector<PublishTag<TagValueTypes...>>&>;
    using WaiterType = decltype(this->job_->get_waiter(this->tags_.front()));
    std::vector<WaiterType> waiters;
    waiters.reserve(this->tags_.size());
    return when_all_same(waiters).then(
      [this](const std::vector<std::optional<ValueType>>& opt_values) noexcept -> RetType {
        std::vector<ValueType> ret_values;
        ret_values.reserve(opt_values.size());
        auto tag_iter = this->tags_.begin();
        for (const auto& val : opt_values) {
          if (val) {
            ret_values.push_back(*val);
            ++tag_iter;
          }
          else {
            this->dead_tags_.push_back(std::move(*tag_iter));
            tag_iter = this->tags_.erase(tag_iter);
          }
        }
        return {ret_values, this->tags_};
      });
  }

private:
  friend class PendingIterativeMethod<SynchronousIterative>;

  SynchronousIterative(
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const std::vector<PublishTag<TagValueTypes...>>& tags) noexcept
    : internal::IterativeBase<TagValueTypes...>{job, produced_tag, tags}
  {}
}; // class SynchronousIterative

template<typename... Args>
auto create_synchronous_iterative(Args&&... args) noexcept
{
  return internal::create_iterative<SynchronousIterative>(std::forward<Args>(args)...);
}
} // namespace skynet

#endif // SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
