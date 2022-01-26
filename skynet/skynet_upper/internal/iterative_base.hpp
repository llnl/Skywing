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


template<typename WaiterType, typename... TagValueTypes>
struct IsIterativeReady
{
  // TODO: this is mutable only because waiter::is_ready is not marked const. should it be?
  mutable WaiterType subscribe_waiter;
  Job& job;
  const PublishTag<TagValueTypes...> produced_tag;
  size_t num_tags;

  mutable bool is_subscribe_done_ = false;

  bool operator()() const noexcept
  {
    return (subscribe_waiter.is_ready() && job.number_of_subscribers(produced_tag) >= num_tags);
    // if (!is_subscribe_done_)
    // {
    //   if (!subscribe_waiter.is_ready())
    //     return false;
    //   is_subscribe_done_ = true;
    // }
    // bool is_ready = (is_subscribe_done_ &&
    //                  (job.number_of_subscribers(produced_tag) >= num_tags));
    // return is_ready;
  }
}; // struct IsIterativeReady

template<typename IterativeMethod, typename Tuple>
struct GetIterativeOpt
{
  // using TagType = typename IterativeMethod::ValueTag;
  // Job& job;
  // const TagType produced_tag;
  // const std::vector<TagType> tags;
  // std::optional<IterativeMethod> operator()() const noexcept
  // {
  //   return IterativeMethod{job, produced_tag, tags};
  // }
  Tuple t;
  std::optional<IterativeMethod> operator()() const noexcept
  {
    return std::make_from_tuple<IterativeMethod>(t);
  }
}; // struct GetIterativeOpt



template<typename IterativeMethod,
         typename WaiterType,
         typename Range,
         typename... ConstructorArgs>
auto create_iterative(
  WaiterType waiter,
  MasterHandle handle,
  Job& job,
  const typename IterativeMethod::ValueTag produced_tag,
  const Range tags,
  ConstructorArgs&&... cons_args) noexcept
{ 
  using ValueType = typename IterativeMethod::ValueType;
  using ValueTag = typename IterativeMethod::ValueTag;
  using ProcT = typename IterativeMethod::ProcessorT;
  using UpdateT = typename IterativeMethod::UpdateNbrsCriterionT;
  using StopT = typename IterativeMethod::StoppingCriterionT;
  
  const auto iter = std::find(tags.cbegin(), tags.cend(), produced_tag);
  if (iter == tags.cend()) {
    std::cerr << "Produced tag for iterative method (ie IterativeBase) not found in the tag list!\n";
    // TODO throw error rather than exit.
    std::exit(2);
  }

  IsIterativeReady<WaiterType, ValueType> iir{std::move(waiter), job, produced_tag, tags.size()};

  auto proc = WaiterValBuilder<ProcT>(std::forward<ConstructorArgs>(cons_args)...).build_waiterval();
  auto updater = WaiterValBuilder<UpdateT>().build_waiterval();
  auto stopper = WaiterValBuilder<StopT>().build_waiterval();
  
  //  auto t1 = std::make_tuple(std::forward<ConstructorArgs>(cons_args)...);
  auto t1 = std::make_tuple(proc.get(), updater.get(), stopper.get());
  
  std::vector<ValueTag> tags_vec(tags.cbegin(), tags.cend());
  auto t2 = std::tuple<Job&, ValueTag, std::vector<ValueTag>>(job, produced_tag, tags_vec);
  auto t = std::tuple_cat(t2, t1);
  GetIterativeOpt<IterativeMethod, decltype(t)> gio{std::move(t)};

  return handle.waiterval_on_subscription_change<std::optional<IterativeMethod>>(std::move(iir), std::move(gio));
}





  
// template<template<typename...> typename IterativeTemplate,
//          typename WaiterType,
//          typename... TagValueTypes,
//          typename Range>
// auto create_iterative(
//   WaiterType waiter,
//   MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const Range& tags) noexcept
// {
//   const auto iter = std::find(tags.cbegin(), tags.cend(), produced_tag);
//   if (iter == tags.cend()) {
//     std::cerr << "Produced tag for iterative method (ie IterativeBase) not found in the tag list!\n";
//     // TODO throw error rather than exit.
//     std::exit(2);
//   }

//   IsIterativeReady<WaiterType, TagValueTypes...> iir{std::move(waiter), job, produced_tag, tags.size()};
//   std::vector<PublishTag<TagValueTypes...>> tags_vec(tags.cbegin(), tags.cend());
//   GetIterativeOpt<IterativeTemplate<TagValueTypes...>> gio{job, produced_tag, tags_vec};
//   return handle.waiter_on_subscription_change(std::move(iir)).then(std::move(gio));
// }

} // namespace skynet::internal

#endif // SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
