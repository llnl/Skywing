#ifndef SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
#define SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP

//#include "skynet_upper/pending_iterative.hpp"
#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"

namespace skynet::internal {
template<typename... TagValueTypes>
class IterativeBase {
public:
  using TagType = PublishTag<TagValueTypes...>;

  const TagType& my_tag() const { return produced_tag_; }
  
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

  Job& get_job() const noexcept { return *job_; }
  
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

} // namespace skynet::internal

#endif // SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
