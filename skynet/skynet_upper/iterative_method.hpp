#ifndef SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
#define SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/pubsub_converter.hpp"
#include "skynet_upper/iterative_helpers.hpp"
#include "skynet_upper/neighbor_data_handler.hpp"

namespace skynet {

/** @brief Base class for iterative methods.
 * 
 * @param ResiliencePolicy Determines how this IterativeMethod
 * should respond to problems such as dead neighbors.
 *
 * @tparam TagValueTypes... Values types this iterative method will
 * publish to neighbors.
 * 
 */
template<typename ResiliencePolicy, typename DataType>
class IterativeMethod {
public:
  using ThisT = IterativeMethod<ResiliencePolicy, DataType>;
  using TagValueType = typename PubSubConverter<DataType>::pubsub_type;
  using TagType = PublishTag<TagValueType>;

  /** @param job The job running this iterative method.
   *  @param produced_tag The tag this agent will publish during iteration.
   *  @param tags The set of tags consumed during iteration, possibly including @p produced_tag
   */
  IterativeMethod(
    Job& job,
    const TagType& produced_tag,
    const std::vector<TagType>& tags,
    ResiliencePolicy resilience_policy) noexcept
    : job_{&job}, produced_tag_{produced_tag}, tags_{tags},
      resilience_policy_(resilience_policy)
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

  const TagType& my_tag() const { return produced_tag_; }

  template<typename TagIter>
  TagIter handle_dead_neighbor(const TagIter& tag_iter) noexcept
  {
    dead_tags_.push_back(std::move(*tag_iter));
    resilience_policy_.handle_dead_neighbor(*this, tag_iter);
    return tags_.erase(tag_iter);
  }
  
  /** @brief Rebuilds connections for dead tags
   */
  Waiter<void> rebuild_dead_tags() noexcept
  {
    auto to_ret = job_->rebuild_tags(dead_tags_);
    std::move(dead_tags_.begin(), dead_tags_.end(), std::back_inserter(tags_));
    dead_tags_.clear();
    return to_ret;
  }

  /** @brief Rebuilds the specified dead tags, ignoring any tags that aren't dead
   */
  template<typename Range>
  Waiter<void> rebuild_dead_tags_range(const Range& r) noexcept
  {
    std::vector<TagType> search_tags;
    for (const auto& tag : r) {
      auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) {
        search_tags.push_back(std::move(*iter));
        dead_tags_.erase(*iter);
      }
    }
    return job_->rebuild_tags(search_tags);
  }

  /** @brief Rebuilds the specified dead tags
   */
  template<typename... Tags>
  Waiter<void> rebuild_dead_tags(const Tags&... tags) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
    return rebuild_dead_tags_range(tag_array);
  }

  /** @brief Drops tracking for dead tags
   */
  void drop_dead_tags() noexcept
  {
    // TODO: Actually unsubscribe when that's a thing that can happen
    dead_tags_.clear();
  }

  /** @brief Drops tracking for specific tags, does nothing if the tags aren't dead
   */
  template<typename Range>
  void drop_dead_tags(const Range& r) noexcept
  {
    for (const auto& tag : r) {
      const auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) { dead_tags_.erase(iter); }
    }
  }

  /** @brief Drops the specified dead tags
   */
  template<typename... Tags>
  void drop_dead_tags(const Tags&... tags) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
  }

  bool gather_values()
  {
    auto tag_iter = tags_.begin();
    size_t num_updated = 0;
    while (tag_iter != tags_.cend())
    {
      const auto& tag = *tag_iter;
      if (!job_->tag_has_active_publisher(tag))
      {
        tag_iter = handle_dead_neighbor(tag_iter);
        continue;
      }
      if (job_->has_data(tag)) num_updated++;
      ++tag_iter;
    }
    if (num_updated == 0) return false;

    updated_tags_.clear();
    updated_tags_.reserve(num_updated);
    for (const auto& tag : tags_)
    {
      if (job_->has_data(tag))
      {
        const auto value_opt = job_->get_waiter(tag).get();
        assert(value_opt);
        neighbor_values_[tag] = PubSubConverter<DataType>::deconvert(*value_opt);
        updated_tags_.push_back(&tag);
      }
    }
    return true;
  }

  /** @brief Publish this agent's values for its neighbors.
   */
  auto submit_values(DataType value_to_submit) noexcept
  {
    job_->publish(produced_tag_, PubSubConverter<DataType>::convert(std::move(value_to_submit)));
  }


  /** @brief Returns all active tags
   */
  const std::vector<TagType>& tags() const noexcept { return tags_; }

  /** @brief Returns all dead tags
   */
  const std::vector<TagType>& dead_tags() const noexcept { return dead_tags_; }

  Job& get_job() const noexcept { return *job_; }

  template<typename ret_type>
  NeighborDataHandler<ThisT, ret_type> get_neighbor_data_handler(std::function<ret_type(DataType& v)> f)
  { return NeighborDataHandler<ThisT, ret_type>(std::move(f), *this); }
                                                                 
  NeighborDataHandler<ThisT, DataType> get_neighbor_data_handler()
  { return get_neighbor_data_handler([](DataType& v){return v;}); }

protected:
  Job* job_;
  TagType produced_tag_;
  std::vector<TagType> tags_;
  std::vector<TagType> dead_tags_;
  ResiliencePolicy resilience_policy_;

private:
  tag_map<TagType, DataType> neighbor_values_;
  std::vector<const TagType*> updated_tags_;

  template<typename Callable, typename IterMethod>
  friend class NeighborDataHandler;
}; // class IterativeBase

} // namespace skynet

#endif // SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
