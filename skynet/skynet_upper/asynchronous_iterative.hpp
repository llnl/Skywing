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
#include <condition_variable>

namespace skynet {

/** \brief Method for receiving information from a specified set of tags
 * in an asynchronous fashion
 */
template<typename Processor, typename UpdateNbrsCriterion, typename StoppingCriterion>
class AsynchronousIterative : public internal::IterativeBase<typename Processor::ValueType>
{
  using ThisT = AsynchronousIterative<Processor, UpdateNbrsCriterion, StoppingCriterion>;
  
public:
  using ValueType = typename Processor::ValueType;
  using ValueTag = skynet::PublishTag<ValueType>;
  using ProcessorT = Processor;
  using UpdateNbrsCriterionT = UpdateNbrsCriterion;
  using StoppingCriterionT = StoppingCriterion;

  AsynchronousIterative(Job& job,
                        const ValueTag& produced_tag,
                        const std::vector<ValueTag>& tags,
                        Processor processor,
                        UpdateNbrsCriterion update_nbrs_criterion,
                        StoppingCriterion stopping_criterion) noexcept
    : internal::IterativeBase<ValueType>{job, produced_tag, tags},
    processor_(std::move(processor)),
    values_(tags.size()),
    is_updated_(tags.size()),
    publish_values_(processor_.get_init_publish_values()),
    update_nbrs_criterion_(std::move(update_nbrs_criterion)),
    stopping_criterion_(std::move(stopping_criterion))
  {
  }

  template<bool has_callback = true>
  void run(std::function<void(const ThisT&)> callback)
  {
    start_time_ = clock_t::now();
    submit_values(publish_values_);
    while (iterate_)
    {
      while (iterate_)
      {
        auto vals = std::move(values()); // an std::optional<std::tuple<...>>
        if (!vals) break;
        
        const auto& [received_values_vec, is_updated, alive_tags] = *vals;
        ++iteration_count_;
        for (size_t vals_ind = 0; vals_ind < received_values_vec.size(); vals_ind++)
        {
          if (is_updated[vals_ind])
            processor_.process_update(alive_tags[vals_ind], received_values_vec[vals_ind], *this);
        }
        ValueType new_vals = processor_.prepare_for_publication(publish_values_);
        if (update_nbrs_criterion_(new_vals, publish_values_))
        {
          publish_values_ = std::move(new_vals);
          submit_values(publish_values_);
        }
        
        if constexpr (has_callback) callback(*this);
        iterate_ = stopping_criterion_(*this);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      this->get_job().wait_for_update(std::chrono::seconds(1));
      iterate_ = stopping_criterion_(*this);
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
  auto values() noexcept -> std::optional<std::tuple<const std::vector<ValueType>&,
                                                     const std::vector<bool>&,
                                                     const std::vector<ValueTag>&>>
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
    bool any_updated = false;
    while (tag_iter != this->tags_.cend()) {
      const auto& tag = *tag_iter;
      if (!this->job_->tag_has_active_publisher(tag)) {
        mark_current_as_dead();
        continue;
      }
      else if (this->job_->has_data(tag)) {
        const auto value_opt = this->job_->get_waiter(tag).get();
        assert(value_opt);
        any_updated = true;
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
    if (any_updated) return std::make_optional(std::make_tuple(std::cref(values_), std::cref(is_updated_), std::cref(this->tags_))); //std::optional<ret_v_t>(std::in_place, values_, is_updated_, this->tags_);
    else return {};
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

  std::chrono::milliseconds run_time() const
  {
    if (!start_time_)
      return std::chrono::milliseconds::zero();
    
    auto curr_time = clock_t::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - *start_time_);
  }

  const ValueType& get_publication_values() const { return publish_values_; }
  
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
  UpdateNbrsCriterion update_nbrs_criterion_;
  StoppingCriterion stopping_criterion_;

  using clock_t = std::chrono::steady_clock;
  std::optional<std::chrono::time_point<clock_t>> start_time_; // only contains a value once the iteration begins

  size_t iteration_count_ = 0;
  bool iterate_ = true;
}; // class AsynchronousIterative




template<typename Processor, typename UpdateNbrsCriterion, typename StoppingCriterion>
class WaiterValBuilder<AsynchronousIterative<Processor, UpdateNbrsCriterion, StoppingCriterion>>
{
public:
  using ObjectT = AsynchronousIterative<Processor, UpdateNbrsCriterion, StoppingCriterion>;
  using ThisT = WaiterValBuilder<ObjectT>;
  using ValueTag = typename Processor::ValueTag;

  template<typename Range>
  WaiterValBuilder(MasterHandle handle, Job& job,
                   const ValueTag& produced_tag,
                   const Range& tags)
    : handle_(handle), job_(job),
      produced_tag_(produced_tag),
      tags_vec_(tags.cbegin(), tags.end())
  {
    job.declare_publication_intent(produced_tag);
    subscribe_waiter_ =
      std::make_shared<Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>>
      (job.subscribe_range(tags));
  }

  template<typename... Args>
  ThisT& set_processor(Args&&... args)
  {
    processor_waiter_ = std::make_shared<WaiterVal<Processor>>
      (std::move(WaiterValBuilder<Processor>(std::forward<Args>(args)...).build_waiterval()));
    return *this;
  }

  template<typename... Args>
  ThisT& set_nbr_update_criterion(Args&&... args)
  {
    nbr_update_criterion_waiter_ = std::make_shared<WaiterVal<UpdateNbrsCriterion>>
      (WaiterValBuilder<UpdateNbrsCriterion>(std::forward<Args>(args)...).build_waiterval());
    return *this;
  }

  template<typename... Args>
  ThisT& set_stopping_criterion(Args&&... args)
  {
    stopping_criterion_waiter_ = std::make_shared<WaiterVal<StoppingCriterion>>
      (WaiterValBuilder<StoppingCriterion>(std::forward<Args>(args)...).build_waiterval());
    return *this;
  }

  ObjectT build()
  {
    return ObjectT(job_, produced_tag_, tags_vec_,
                   std::move(processor_waiter_->get()),
                   std::move(nbr_update_criterion_waiter_->get()),
                   std::move(stopping_criterion_waiter_->get()));
  }

  WaiterVal<ObjectT> build_waiterval()
  {
    // capture by value to ensure liveness of shared ptrs
    auto is_ready = [subscribe_waiter_ = this->subscribe_waiter_,
                     processor_waiter = this->processor_waiter_,
                     nbr_update_criterion_waiter = this->nbr_update_criterion_waiter_,
                     stopping_criterion_waiter = this->stopping_criterion_waiter_]()
      {
        return (subscribe_waiter_->is_ready()
                && processor_waiter->is_ready()
                && nbr_update_criterion_waiter->is_ready()
                && stopping_criterion_waiter->is_ready());
      };
    
    auto cons_args = std::make_tuple
      (std::ref(job_), produced_tag_, tags_vec_,
       processor_waiter_->get(),
       nbr_update_criterion_waiter_->get(),
       stopping_criterion_waiter_->get());
    auto get_object = [cons_args = std::move(cons_args)]()
        { return std::make_from_tuple<ObjectT>(cons_args); };
    return handle_.waiterval_on_subscription_change<ObjectT>(is_ready, std::move(get_object));
  }


private:
  MasterHandle handle_;
  Job& job_;
  ValueTag produced_tag_;
  std::vector<ValueTag> tags_vec_;

  // Using shared_ptrs on these waiters so that they are not destroyed
  // if the WaiterValBuilder gets destroyed before the
  // object is retrieved from the Waiter<ThisT>.
  std::shared_ptr<Waiter<internal::MasterSubscribeIsDone, WaiterGetNoOp>> subscribe_waiter_;
  std::shared_ptr<WaiterVal<Processor>> processor_waiter_;
  std::shared_ptr<WaiterVal<UpdateNbrsCriterion>> nbr_update_criterion_waiter_;
  std::shared_ptr<WaiterVal<StoppingCriterion>> stopping_criterion_waiter_;
}; // class WaiterValBuilder<...>


} // namespace skynet

#endif // SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
