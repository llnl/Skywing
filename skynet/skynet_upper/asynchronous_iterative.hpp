#ifndef SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/iterative_method.hpp"
#include "skynet_upper/stop_policies.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <condition_variable>

namespace skynet {
using namespace std::chrono_literals;
  
/**
 * @brief A decentralized, fully-asynchronous iterative method.
 * 
 * This class template implements the framework of an iterative method
 * in which an agent performs a local computation, and possibly sends
 * updates to neighbors, as soon as it receives any update. It does
 * not wait for all neighbors to send updates.
 *
 * Can be constructed directly, but in most cases is easiest to build
 * through the WaiterBuilder class.
 *
 * @tparam Processor The numerical heart of the iterative method. Must
 * define a type @p ValueType of the data type to communicate, as well
 * as the following member functions: 
 * - @p ValueType get_init_publish_values()
 * - @p void template<typename CallerT> process_update(const std::vector<ValueTag>&, const std::vector<ValueType>&, const CallerT&)
 * - @p ValueType prepare_for_publication(ValueType)
 *
 * @tparam PublishPolicy Determines when it is worth sending an
 * update to its neighbors. Must define a member function 
 * @p bool operator()(const ValueType& new_vals, const ValueType& old_vals)
 *
 * @tparam StopPolicy Determines when to stop the
 * iteration. Must define a member function @ bool operator()(constCallerT&)
 */
template<typename Processor, typename PublishPolicy, typename StopPolicy>
class AsynchronousIterative : public IterativeMethod<typename Processor::ValueType>
{
  using ThisT = AsynchronousIterative<Processor, PublishPolicy, StopPolicy>;
  
public:
  using ValueType = typename Processor::ValueType;
  using ValueTag = skynet::PublishTag<ValueType>;
  using ProcessorT = Processor;
  using PublishPolicyT = PublishPolicy;
  using StopPolicyT = StopPolicy;

    /**
   * @param job The job running the iteration.
   * @param produced_tag The tag of the data produced by this agent and sent to iteration neighbors.
   * @param tags The set of tags with <em>already finalized subscriptions</em> from neighbors this iteration relies on.
   * @param processor The Processor object used in iteration.
   * @param publish_policy The PublishPolicy object used in iteration.
   * @param stop_policy The StopPolicy object used in iteration.
   * @param loop_delay_max The maximum amount of time to wait for an update before at least checking the stopping criterion.
   */
  AsynchronousIterative(
    Job& job,
    const ValueTag& produced_tag,
    const std::vector<ValueTag>& tags,
    Processor processor,
    PublishPolicy publish_policy,
    StopPolicy stop_policy,
    std::chrono::milliseconds loop_delay_max = 1000ms) noexcept
    : IterativeMethod<ValueType>{job, produced_tag, tags},
    processor_(std::move(processor)),
    publish_values_(processor_.get_init_publish_values()),
    publish_policy_(std::move(publish_policy)),
    stop_policy_(std::move(stop_policy)),
    wait_max_(loop_delay_max)
  { }

  /** @brief Run the iteration until stopping time or forever.
   *  @param callback A callback function to call after each processing iteration.
   */ 
  template<bool has_callback = true>
  void run(std::function<void(const ThisT&)> callback)
  {
    start_time_ = clock_t::now();
    submit_values(publish_values_);
    iterate_ = true;
    while (iterate_)
    {
      while (iterate_)
      {
        auto vals = values(); // an std::optional<std::pair<...>>
        if (!vals) break;

        const auto [received_values_vec, nbr_tags] = *vals;
        processor_.process_update(nbr_tags, received_values_vec, *this);
        
        ValueType new_vals = processor_.prepare_for_publication(publish_values_);
        if (publish_policy_(new_vals, publish_values_))
        {
          publish_values_ = std::move(new_vals);
          submit_values(publish_values_);
        }
        
        if constexpr (has_callback) callback(*this);
        iterate_ = stop_policy_(*this);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      if (!iterate_) break;
      this->get_job().wait_for_update(wait_max_);
      iterate_ = stop_policy_(*this);
    }
    stop_time_ = clock_t::now();
  }

  /** @brief Run the iteration until stopping time or forever without a callback.
   */ 
  void run()
  {
    // Call run with has_callback=false so the callback doesn't get
    // called. The actual lambda passed in doesn't matter.
    run<false>([](const ThisT&) { return; } );
  }

  /** @brief Returns values from all tags that have been updated.
   *
   *  @returns If any neighbor data has been updated, an optional
   *  containing a pair of vectors of values and associated
   *  tags. Otherwise, an empty optional.
   */
  auto values() noexcept -> std::optional<std::pair<std::vector<ValueType>,
                                                    std::vector<ValueTag>>>
  {
    auto tag_iter = this->tags_.begin();
    const auto mark_current_as_dead = [&]() {
      this->dead_tags_.push_back(std::move(*tag_iter));
      tag_iter = this->tags_.erase(tag_iter);
    };
    size_t num_updated = 0;
    while (tag_iter != this->tags_.cend())
    {
      const auto& tag = *tag_iter;
      if (!this->job_->tag_has_active_publisher(tag))
      {
        mark_current_as_dead();
        continue;
      }
      if (this->job_->has_data(tag)) num_updated++;
      ++tag_iter;
    }
    if (num_updated == 0) return {};
    
    std::vector<ValueType> nbr_values;
    std::vector<ValueTag> nbr_tags;
    nbr_values.reserve(num_updated);
    nbr_tags.reserve(num_updated);
    for (const auto& tag : this->tags_)
    {
      if (this->job_->has_data(tag))
      {
        const auto value_opt = this->job_->get_waiter(tag).get();
        assert(value_opt);
        nbr_values.push_back(*value_opt);
        nbr_tags.push_back(tag);
      }
    }
    return std::make_optional(std::make_pair(std::move(nbr_values), std::move(nbr_tags)));
  }

  /** @brief Publish this agent's values for its neighbors.
   */
  template<typename... ArgTypes>
  auto submit_values(ArgTypes&&... values_to_submit) noexcept
  {
    this->job_->publish(this->produced_tag_, std::forward<ArgTypes>(values_to_submit)...);
  }

  /** @brief Get iteration run time, or zero if not yet began.
   */
  std::chrono::milliseconds run_time() const
  {
    if (!start_time_)
      return std::chrono::milliseconds::zero();
    if (!iterate_)
      return std::chrono::duration_cast<std::chrono::milliseconds>(*stop_time_ - *start_time_);
    
    auto curr_time = clock_t::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - *start_time_);
  }

  const ValueType& get_publication_values() const { return publish_values_; }

  /** @brief Get number of iterations.
   */
  unsigned get_iteration_count() const
  {
    return iteration_count_;
  }

  /** @brief Get if iteration is ongoing. False can mean either that
   *   it has stopped or that it has not yet begun.
   */
  bool return_iterate() const
  {
    return iterate_;
  }

  Processor& get_processor() { return processor_; }
  const Processor& get_processor() const { return processor_; }

private:
  Processor processor_;
  ValueType publish_values_;
  PublishPolicy publish_policy_;
  StopPolicy stop_policy_;

  using clock_t = std::chrono::steady_clock;
  std::optional<std::chrono::time_point<clock_t>> start_time_; // only contains a value once the iteration begins
  std::optional<std::chrono::time_point<clock_t>> stop_time_; // only contains a value once the iteration ends

  size_t iteration_count_ = 0;
  bool iterate_ = false;
  std::chrono::milliseconds wait_max_;
}; // class AsynchronousIterative




/** @brief A template specialization of WaiterBuilder for AsynchronousIterative methods.
 *
 * To build this WaiterBuilder, do not pass the AsynchronousIterative
 * template parameters directly, but define the specific type of
 * AsynchronousIterative you wish to build and pass that. Then call
 * each of the @p set_* member functions, passing constructor
 * parameters as needed (still call it even if the parameter list is
 * empty), and finally call @p build_waiter to obtain a Waiter to your
 * iterative method. This waiter will wait on any subscriptions or
 * other wait conditions that need to be completed, and will then
 * lazily construct each sub-component and finally the iterative
 * method itself.
 *
 * For example, a typical use might be as follows:
 * @code
 * using IterMethod = AsynchronousIterative<JacobiProcessor<double>, AlwaysUpdateNbrs, StopAfterTime>;
 * Waiter<IterMethod> iter_waiter =
 *  WaiterBuilder<IterMethod>(master_handle, job, my_tag, nbr_tags)
 *  .set_processor(A, b, row_inds)
 *  .set_nbr_update_criterion()
 *  .set_stop_policy(std::chrono::seconds(5))
 *  .build_waiter();
 * IterMethod sync_jacobi = iter_waiter.get();
 * @endcode
 */  
template<typename Processor, typename PublishPolicy, typename StopPolicy>
class WaiterBuilder<AsynchronousIterative<Processor, PublishPolicy, StopPolicy>>
{
public:
  using ObjectT = AsynchronousIterative<Processor, PublishPolicy, StopPolicy>;
  using ThisT = WaiterBuilder<ObjectT>;
  using ValueTag = typename Processor::ValueTag;

    /**
   * @param handle MasterHandle object running this agent.
   * @param job The job running the iteration.
   * @param produced_tag The tag of the data produced by this agent and sent to iteration neighbors.
   * @param tags An iteration-capable container of tags of neighboring data from which this agent will collect updates. Can be 
   */
  template<typename Range>
  WaiterBuilder(MasterHandle handle, Job& job,
                   const ValueTag& produced_tag,
                   const Range& tags)
    : handle_(handle), job_(job),
      produced_tag_(produced_tag),
      tags_vec_(tags.cbegin(), tags.end())
  {
    job.declare_publication_intent(produced_tag);
    subscribe_waiter_ =
      std::make_shared<Waiter<void>>(job.subscribe_range(tags));
  }

  /** @brief Build a Waiter<Processor> that will construct the Processor for this iterative method.
   */
  template<typename... Args>
  ThisT& set_processor(Args&&... args)
  {
    processor_waiter_ = std::make_shared<Waiter<Processor>>
      (std::move(WaiterBuilder<Processor>(std::forward<Args>(args)...).build_waiter()));
    return *this;
  }

  /** @brief Build a Waiter<PublishPolicy> that will construct the PublishPolicy for this iterative method.
   */
  template<typename... Args>
  ThisT& set_publish_policy(Args&&... args)
  {
    publish_policy_waiter_ = std::make_shared<Waiter<PublishPolicy>>
      (WaiterBuilder<PublishPolicy>(std::forward<Args>(args)...).build_waiter());
    return *this;
  }

  /** @brief Build a Waiter<StopPolicy> that will construct the StopPolicy for this iterative method.
   */
  template<typename... Args>
  ThisT& set_stop_policy(Args&&... args)
  {
    stop_policy_waiter_ = std::make_shared<Waiter<StopPolicy>>
      (WaiterBuilder<StopPolicy>(std::forward<Args>(args)...).build_waiter());
    return *this;
  }
  
  /* @brief Build a Waiter to the desired synchronous iterative method.
   * @returns A Waiter<AsynchronousIterative<Processor, PublishPolicy, StopPolicy>>
   */
  Waiter<ObjectT> build_waiter()
  {
    // capture by value to ensure liveness of shared ptrs
    auto is_ready = [subscribe_waiter_ = this->subscribe_waiter_,
                     processor_waiter = this->processor_waiter_,
                     publish_policy_waiter = this->publish_policy_waiter_,
                     stop_policy_waiter = this->stop_policy_waiter_]()
      {
        return (subscribe_waiter_->is_ready()
                && processor_waiter->is_ready()
                && publish_policy_waiter->is_ready()
                && stop_policy_waiter->is_ready());
      };
    
    auto cons_args = std::make_tuple
      (std::ref(job_), produced_tag_, tags_vec_,
       processor_waiter_->get(),
       publish_policy_waiter_->get(),
       stop_policy_waiter_->get());
    auto get_object = [cons_args = std::move(cons_args)]()
        { return std::make_from_tuple<ObjectT>(cons_args); };
    return handle_.waiter_on_subscription_change<ObjectT>(is_ready, std::move(get_object));
  }


private:
  MasterHandle handle_;
  Job& job_;
  ValueTag produced_tag_;
  std::vector<ValueTag> tags_vec_;

  // Using shared_ptrs on these waiters so that they are not destroyed
  // if the WaiterBuilder gets destroyed before the
  // object is retrieved from the Waiter<ThisT>.
  std::shared_ptr<Waiter<void>> subscribe_waiter_;
  std::shared_ptr<Waiter<Processor>> processor_waiter_;
  std::shared_ptr<Waiter<PublishPolicy>> publish_policy_waiter_;
  std::shared_ptr<Waiter<StopPolicy>> stop_policy_waiter_;
}; // class WaiterBuilder<...>


} // namespace skynet

#endif // SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
