#ifndef SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/internal/iterative_base.hpp"
#include "skynet_upper/stopping_criterion.hpp"

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
template<typename Processor>
class AsynchronousIterative : public internal::IterativeBase<typename Processor::ValueType>
{
  using ThisT = AsynchronousIterative<Processor>;
  
public:
  using ValueType = typename Processor::ValueType;
  using ValueTag = skynet::PublishTag<ValueType>;

  template<typename... Args>
  AsynchronousIterative(Job& job,
                        const ValueTag& produced_tag,
                        const std::vector<ValueTag>& tags,
                        Args&&... args) noexcept
    : internal::IterativeBase<ValueType>{job, produced_tag, tags},
    processor_(*this, std::forward<Args>(args)...),
    values_(tags.size()),
    is_updated_(tags.size()),
    publish_values_(processor_.get_init_publish_values())
  {
    max_run_time_ = std::chrono::seconds(5);
    submit_values(publish_values_);
  }

  template<bool has_callback = true>
  void run(std::function<void(const ThisT&)> callback)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    while(iterate_)
    {
      ++iteration_count_;
      const auto& [received_values_vec, is_updated, alive_tags] = values();
      for (size_t vals_ind = 0; vals_ind < received_values_vec.size(); vals_ind++)
      {
        if (is_updated[vals_ind])
        {
          processor_.process_update(alive_tags[vals_ind], received_values_vec[vals_ind], *this);
          processor_.prepare_for_publication(publish_values_);
          submit_values(publish_values_);
        }
      }
      
      auto curr_time = std::chrono::high_resolution_clock::now();
      run_time_ = std::chrono::duration_cast<std::chrono::microseconds>(curr_time - start_time);
      iterate_ = should_stop(run_time_, max_run_time_);

      if constexpr (has_callback)
                     callback(*this);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  void run()
  {
    // Call run with has_callback=false so the callback doesn't get
    // called. The actual lambda passed in doesn't matter.
    run<false>([](const ThisT&) { return; } );
  }

  /** \brief Returns values from all tags without submitting a value
   */
  auto values() noexcept -> std::tuple<const std::vector<ValueType>&,
                                       const std::vector<bool>&,
                                       const std::vector<ValueTag>&>
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
                  const std::vector<ValueTag>&>
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

  double return_run_time() const
  {
    return run_time_.count();
  }
  
  unsigned get_iteration_count() const
  {
    return iteration_count_;
  }

  bool return_iterate() const
  {
    return iterate_;
  }

  Processor& get_processor() { return processor_; }
  const Processor& get_processor() const { return processor_; }

private:
  Processor processor_;
  std::vector<ValueType> values_;
  std::vector<bool> is_updated_;
  ValueType publish_values_;

  std::chrono::duration<double> max_run_time_;
  std::chrono::duration<double> run_time_;
  size_t iteration_count_ = 0;
  bool iterate_ = true;
}; // class AsynchronousIterative



template<typename ConcreteAsynchronous, typename Range, typename... CAArgs>
auto create_asynchronous_iterative(
    MasterHandle handle,
    Job& job,
    const typename ConcreteAsynchronous::ValueTag& produced_tag,
    const Range& tags,
    CAArgs&&... args) noexcept
{
  job.declare_publication_intent(produced_tag);
  return internal::create_iterative<ConcreteAsynchronous, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
    job.subscribe_range(tags),
    handle,
    job,
    produced_tag,
    tags,
    std::forward<CAArgs>(args)...
  );
}
  









  
// template<typename... TagValueTypes,
//          typename Range>
// auto create_asynchronous_iterative(
//   MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const Range& tags) noexcept
// {
//   job.declare_publication_intent(produced_tag);
//   return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
//     job.subscribe_range(tags),
//     handle,
//     job,
//     produced_tag,
//     tags
//   );
// }

// template<typename... TagValueTypes, typename... TagTypes>
// auto create_asynchronous_iterative(
//   MasterHandle handle, Job& job, const PublishTag<TagValueTypes...>& produced_tag, const TagTypes&... tags) noexcept
// {
//   job.declare_publication_intent(produced_tag);
//   return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
//     job.subscribe(tags...),
//     handle,
//     job,
//     produced_tag,
//     tags...
//   );
// }

// template<
//   typename... TagValueTypes,
//   typename Range,
//   typename Rep,
//   typename Period>
// auto create_asynchronous_iterative(
//   const std::chrono::time_point<Rep, Period>& end_time,
//   IterativeInitErrorPolicy policy,
//   MasterHandle handle,
//   Job& job,
//   const PublishTag<TagValueTypes...>& produced_tag,
//   const Range& tags) noexcept
// {
//   job.declare_publication_intent(produced_tag);
//   return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
//     job.subscribe_range(tags),
//     end_time,
//     policy,
//     handle,
//     job,
//     produced_tag,
//     tags
//   );
// }

// template<
//   typename... TagValueTypes,
//   typename... TagTypes,
//   typename Rep,
//   typename Period>
// auto create_asynchronous_iterative(
//   const std::chrono::time_point<Rep, Period>& end_time,
//   IterativeInitErrorPolicy policy,
//   MasterHandle handle,
//   Job& job,
//   const PublishTag<TagValueTypes...>& produced_tag,
//   const TagTypes&... tags) noexcept
// {
//   job.declare_publication_intent(produced_tag);
//   return internal::create_iterative<AsynchronousIterative, Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>(
//     job.subscribe(tags...),
//     end_time,
//     policy,
//     handle,
//     job,
//     produced_tag,
//     tags...
//   );
// }
} // namespace skynet

#endif // SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
