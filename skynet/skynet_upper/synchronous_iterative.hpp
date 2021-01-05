#ifndef SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/internal/devices/socket_communicator.hpp"
#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/internal/iterative_base.hpp"
#include "skynet_upper/pending_iterative.hpp"

#include <map>
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
  template<typename, typename>
  friend class PendingIterativeMethod;

  template<typename...>
  friend class SupernodeSynchronousIterative;

  SynchronousIterative(
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const std::vector<PublishTag<TagValueTypes...>>& tags) noexcept
    : internal::IterativeBase<TagValueTypes...>{job, produced_tag, tags}
  {}
}; // class SynchronousIterative

template<typename... TagValueTypes>
class SupernodeSynchronousIterative {
public:
  using ValueType = ValueOrTuple<TagValueTypes...>;

  SupernodeSynchronousIterative(std::map<PrivateTag<TagValueTypes...>, std::vector<std::string>> nodes, SynchronousIterative<TagValueTypes...> sync_iter) noexcept
    : nodes_{std::move(nodes)}
    , sync_iter_{std::move(sync_iter)}
  {}

  /** \brief Retrieves the values from all tags, not being ready
   * until all tags have either received values or errored.
   *
   * If a subscription for a tag become unavailable, the waiter will
   * return an empty vector.
   */
  template<typename... ArgTypes>
  auto values(ArgTypes&&... submit_values) noexcept
  {
    return sync_iter_.values(std::forward<ArgTypes>(submit_values)...).then([this](const auto& base_vals) -> std::vector<ValueType> {
      if (base_vals.empty()) { return {}; }
      std::vector<ValueType> ret_values;
      ret_values.reserve(nodes_.size());
      std::size_t value_loc = 0;
      for (const auto& [tag, node_addresses] : nodes_) {
        (void)tag;
        const ValueType node_value = base_vals[value_loc];
        for (const auto& tag_producer : node_addresses) {
          // We don't do anything with this for now
          (void)tag_producer;
          // There's a difference with the produced values for everything, return error
          if (base_vals[value_loc] != node_value) {
            return {};
          }
          ++value_loc;
        }
        ret_values.push_back(node_value);
      }
      assert(value_loc == base_vals.size());
      return ret_values;
    });
  }

private:
  std::map<PrivateTag<TagValueTypes...>, std::vector<std::string>> nodes_;
  SynchronousIterative<TagValueTypes...> sync_iter_;
}; // class SupernodeSynchronousIterative

template<typename... TagValueTypes>
auto create_supernode_synchronous_iterative(
  MasterHandle handle, Job& job, const PrivateTag<TagValueTypes...>& produced_tag, const std::map<PrivateTag<TagValueTypes...>, std::vector<std::string>>& nodes)
{
  using ProducedType = std::optional<SupernodeSynchronousIterative<TagValueTypes...>>;
  using BaseWaiter = Waiter<internal::MasterIPSubscribeComplete, internal::MasterIPSubscribeSuccess>;
  std::vector<BaseWaiter> waiters;
  std::vector<PublishTag<TagValueTypes...>> all_tags;
  const auto make_new_tag = [&](const auto& tag, const std::string_view addr) {
    const auto canonical_addr = internal::to_canonical(internal::split_address(addr));
    return PrivateTag<TagValueTypes...>{tag.id() + canonical_addr.first + std::to_string(canonical_addr.second)};
  };
  const auto self_addr = "localhost:" + std::to_string(handle.port());
  const auto self_tag = make_new_tag(produced_tag, self_addr);
  job.declare_publication_intent(self_tag);
  for (const auto& [tag, addresses] : nodes) {
    for (const auto& addr : addresses) {
      const auto new_tag = make_new_tag(tag, addr);
      waiters.push_back(job.ip_subscribe(addr, new_tag));
      all_tags.push_back(new_tag);
    }
  }
  return internal::create_iterative<SynchronousIterative, AllWaiterSame<BaseWaiter>>(
    when_all_same(waiters),
    handle,
    job,
    self_tag,
    all_tags
    ).then([nodes](auto sync_iter) -> ProducedType {
      if (sync_iter) {
        return SupernodeSynchronousIterative<TagValueTypes...>{nodes, std::move(*sync_iter)};
      }
      return {};
    });
}

template<typename... TagValueTypes, typename Range>
auto create_synchronous_iterative(
  MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const Range& tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<SynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
    job.subscribe_range(tags),
    handle,
    job,
    produced_tag,
    tags
  );
}

template<typename... TagValueTypes, typename... TagTypes>
auto create_synchronous_iterative(
  MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const TagTypes&... tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<SynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
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
auto create_synchronous_iterative(
  const std::chrono::time_point<Rep, Period>& end_time,
  IterativeInitErrorPolicy policy,
  MasterHandle handle,
  Job& job,
  const PublishTag<TagValueTypes...>& produced_tag,
  const Range& tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<SynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
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
auto create_synchronous_iterative(
  const std::chrono::time_point<Rep, Period>& end_time,
  IterativeInitErrorPolicy policy,
  MasterHandle handle,
  Job& job,
  const PublishTag<TagValueTypes...>& produced_tag,
  const TagTypes&... tags) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<SynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
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

#endif // SKYNET_UPPER_SYNCHRONOUS_ITERATIVE_HPP
