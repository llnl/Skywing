#ifndef SKYWING_MID_SYNCHRONOUS_ITERATIVE_HPP
#define SKYWING_MID_SYNCHRONOUS_ITERATIVE_HPP

#include <chrono>
#include <iostream>
#include <map>
#include <utility>

#include "skywing_core/internal/devices/socket_communicator.hpp"
#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"
#include "skywing_mid/iterative_method.hpp"
#include "skywing_mid/iterative_resilience_policies.hpp"

namespace skywing
{
using namespace std::chrono_literals;

/**
 * @brief Wrapper for processors that require synchronous iteration.
 *
 * This class is designed to wrap processors that require synchronous
 * iteration such that each agent requires exactly the iterate info from
 * iteration (i-1) from its neighbors in order to compute iteration i.
 * This is not explicitly guaranteed if using the synchronous iterative
 * method without this wrapper, e.g. neighbors may have already computed
 * and published info from iteration i by the time the agent reads its
 * subscription (thus receiving info from one iteration ahead of what's
 * expected). This class overrides the publication routines and the main
 * process update routine in order to package two iterates at a time for
 * publication (the current and previous iterate) as well as the current
 * iteration count. Thus, when receiving information, an agent can
 * determine which iterate is the appropriate one to use based on its
 * own stored iteration count. No change is required for the underlying
 * processors. For use of this class, see:
 * examples/sync_jacobi/sync_jacobi.cpp.
 *
 */

template <template<typename...> class Processor, typename... Args>
class ProcessorSyncWrapper
    : public Processor<Args...>
{
public:
    using ValueType = std::tuple<int, typename Processor<Args...>::ValueType, typename Processor<Args...>::ValueType>;

    template <typename... UArgs>
    ProcessorSyncWrapper(UArgs&&... args) :
        Processor<Args...>(std::forward<UArgs>(args)...),
        prev_iterate_(Processor<Args...>::get_init_publish_values()),
        iteration_count_(0)
    {}

    ValueType get_init_publish_values()
    {
        return ValueType(iteration_count_, prev_iterate_, Processor<Args...>::get_init_publish_values());
    }

    template <typename IterMethod>
    void process_update(const DataHandler<ValueType>& wrapper_data_handler,
                        [[maybe_unused]] const IterMethod& iter_method)
    {
        std::unordered_map<std::string, typename Processor<Args...>::ValueType> p_handler_update_map;
        for (const auto& pTag : wrapper_data_handler.recvd_data_tags() ) {
            ValueType nbr_value = wrapper_data_handler.get_data(pTag);
            if (std::get<0>(nbr_value) == iteration_count_) {
                p_handler_update_map[pTag] = std::get<2>(nbr_value);
            }
            else {
                p_handler_update_map[pTag] = std::get<1>(nbr_value);
            }
        }
        processor_data_handler_.update(p_handler_update_map);
        prev_iterate_ = Processor<Args...>::prepare_for_publication(prev_iterate_);
        Processor<Args...>::process_update(processor_data_handler_, iter_method);
        iteration_count_++;
    }

    ValueType prepare_for_publication(ValueType vals_to_publish)
    {
        return ValueType(iteration_count_, prev_iterate_, Processor<Args...>::prepare_for_publication(std::get<2>(vals_to_publish)));
    }

private:
    typename Processor<Args...>::ValueType prev_iterate_;
    DataHandler<typename Processor<Args...>::ValueType> processor_data_handler_;
    int iteration_count_;
};

/**
 * @brief A decentralized iterative method with synchronized rounds.
 *
 * This class template implements the framework of an iterative method
 * in which an agent waits to receive updates from @a all of the
 * subscriptions associated with the method (i.e. all of its
 * algorithmic neighbors) before performing a local
 * iteration. Although this imposes eventual global synchronization
 * constraints across the paricipating Skywing network, it is still
 * decentralized because it only assumes that an agent can communicate
 * with its immediate neigbhors.
 *
 * Can be constructed directly, but in most cases is easiest to build
 * through the WaiterBuilder class specialization.
 *
 * @tparam Processor The numerical heart of the iterative method. Must
 define a type @p ValueType of the data type to communicate, as well
 as the following member functions:
 * - @p ValueType get_init_publish_values()
 * - @p void template<typename CallerT> process_update(const
 std::vector<ValueTag>&, const std::vector<ValueType>&, const CallerT&)
 * - @p ValueType prepare_for_publication(ValueType)
 *
 * @tparam IterationPolicy Determines when to stop the
 * iteration. Must define a member function @ bool operator()(constCallerT&)
 *
 * @tparam ResiliencePolicy Determines how this iterative method
 * should respond to problems such as dead neighbors.
 */
template <typename Processor, typename IterationPolicy, typename ResiliencePolicy>
class SynchronousIterative
    : public IterativeMethod<
          ResiliencePolicy,
          TupleOfValueTypes_t<Processor, IterationPolicy, ResiliencePolicy>>
{
public:
    using BaseT = IterativeMethod<
        ResiliencePolicy,
        TupleOfValueTypes_t<Processor, IterationPolicy, ResiliencePolicy>>;
    using ThisT = SynchronousIterative<Processor, IterationPolicy, ResiliencePolicy>;

    using ValueType = typename BaseT::ValueType;
    using TagType = typename BaseT::TagType;

    using ProcessorT = Processor;
    using IterationPolicyT = IterationPolicy;
    using ResiliencePolicyT = ResiliencePolicy;

    /**
     * @param job The job running the iteration.
     * @param produced_tag The tag of the data produced by this agent and sent
     * to iteration neighbors.
     * @param tags The set of tags with <em>already finalized subscriptions</em>
     * from neighbors this iteration relies on.
     * @param processor The Processor object used in iteration.
     * @param iteration_policy The IterationPolicy object used in iteration.
     * @param resilience_policy The ResiliencePolicy object used in iteration.
     * @param loop_delay_max The maximum amount of time to wait for an update
     * before at least checking the stopping criterion.
     */
    SynchronousIterative(
        Job& job,
        const TagType& produced_tag,
        const std::vector<TagType>& tags,
        Processor processor,
        IterationPolicy iteration_policy,
        ResiliencePolicy resilience_policy,
        std::chrono::milliseconds loop_delay_max = 1000ms,
        std::chrono::milliseconds wait_for_vals_max = 5000ms) noexcept
        : BaseT{job, produced_tag, tags, std::move(resilience_policy)},
          processor_(std::move(processor)),
          publish_values_(gather_initial_publications_()),
          iteration_policy_(std::move(iteration_policy)),
          loop_delay_max_(loop_delay_max),
          wait_for_vals_max_(wait_for_vals_max)
    {}

    /** @brief Run the iteration until stopping time or forever.
     *  @param callback A callback function to call after each processing
     * iteration.
     */
    template <bool has_callback = true>
    void run(std::function<void(ThisT&)> callback)
    {
        start_time_ = clock_t::now();
        this->submit_values(publish_values_);
        should_iterate_ = true;
        while (should_iterate_) {
            while (should_iterate_) {
                wait_for_values_();
                if (!waitervec_->is_ready())
                    break;
                this->gather_values();

                //        processor_.process_update(get_processor_data_handler(),
                //        *this);
                process_all_updates_();
                ++iteration_count_;

                publish_values_ = gather_data_for_publication_();
                this->submit_values(publish_values_);

                if constexpr (has_callback)
                    callback(*this);
                should_iterate_ = iteration_policy_(*this);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (!should_iterate_)
                break;
            this->get_job().wait_for_update(loop_delay_max_);
            should_iterate_ = iteration_policy_(*this);
        }
        stop_time_ = clock_t::now();
    }

    /** @brief Run the iteration until stopping time or forever without
        a callback.
     */
    void run()
    {
        // Call run with has_callback=false so the callback doesn't get
        // called. The actual lambda passed in doesn't matter.
        run<false>([](const ThisT&) { return; });
    }

    /** @brief Get iteration run time, or zero if not yet began.
     */
    std::chrono::milliseconds run_time() const
    {
        if (!start_time_)
            return std::chrono::milliseconds::zero();
        if (!should_iterate_)
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                *stop_time_ - *start_time_);

        auto curr_time = clock_t::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            curr_time - *start_time_);
    }

    /** @brief Get number of iterations.
     */
    unsigned get_iteration_count() const { return iteration_count_; }

    /** @brief Get if iteration is ongoing. False can mean either that
     *   it has stopped or that it has not yet begun.
     */
    bool return_iterate() const { return should_iterate_; }

    Processor& get_processor() { return processor_; }
    const Processor& get_processor() const { return processor_; }

private:
    /** @brief Process all updates for the main processor and the policies.
     *
     *  If a policy has defined a ValueType, then it is an auxiliary
     *  processor that implements the same interface as Processor, and
     *  so processes updates. If it does not define ValueType, then
     *  nothing is done with that policy.
     */
    void process_all_updates_()
    {
        processor_.process_update(
            this->template get_policy_data_handler<Processor, ThisT>(), *this);
        this->template process_policy_update_<IterationPolicy, ThisT>(
            iteration_policy_, std::bool_constant<has_ValueType_v<IterationPolicy>>{});
        this->template process_policy_update_<ResiliencePolicy, ThisT>(
            this->resilience_policy_,
            std::bool_constant<has_ValueType_v<ResiliencePolicy>>{});
    }

    /** @brief Collect values for initial publication from all policies.
     *
     * Any policy that contributes is asked, any policy that doesn't is not.
     */
    ValueType gather_initial_publications_()
    {
        return std::tuple_cat(
            this->template get_init_tuple_<Processor, ThisT>(processor_),
            this->template get_init_tuple_<IterationPolicy, ThisT>(iteration_policy_),
            this->template get_init_tuple_<ResiliencePolicy, ThisT>(
                this->resilience_policy_));
    }

    /** @brief Collect values for publication from all policies.
     *
     * Any policy that contributes is asked, any policy that doesn't is not.
     */
    ValueType gather_data_for_publication_()
    {
        return std::tuple_cat(
            this->template get_pub_tuple_<Processor, ThisT>(processor_,
                                                            publish_values_),
            this->template get_pub_tuple_<IterationPolicy, ThisT>(iteration_policy_,
                                                             publish_values_),
            this->template get_pub_tuple_<ResiliencePolicy, ThisT>(
                this->resilience_policy_, publish_values_));
    }

    using pubval_t = typename TagType::ValueType;

    /** @brief Wait up to @p wait_max_ time for values to be ready.
     */
    void wait_for_values_()
    {
        std::vector<Waiter<std::optional<pubval_t>>> waiters;
        waiters.reserve(this->tags_.size());
        for (const auto& tag : this->tags_) {
            waiters.push_back(this->job_->get_waiter(tag));
        }
        waitervec_ = make_waitervec(std::move(waiters));
        waitervec_->wait_for(wait_for_vals_max_);
    }

    Processor processor_;
    ValueType publish_values_;
    IterationPolicy iteration_policy_;

    using clock_t = std::chrono::steady_clock;
    std::optional<std::chrono::time_point<clock_t>>
        start_time_; // only contains a value once the iteration begins
    std::optional<std::chrono::time_point<clock_t>>
        stop_time_; // only contains a value once the iteration ends

    size_t iteration_count_ = 0;
    bool should_iterate_ = false;
    std::optional<WaiterVec<std::optional<pubval_t>>> waitervec_;
    std::chrono::milliseconds loop_delay_max_;
    std::chrono::milliseconds wait_for_vals_max_;
}; // class SynchronousIterative

/** @brief A template specialization of WaiterBuilder for
 * SynchronousIterative methods.
 *
 * To build this WaiterBuilder, do not pass the Processor and
 * IterationPolicy parameters directly, but define the specific type
 * of SynchronousIterative you wish to build and pass that. Then call
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
 * using IterMethod = SynchronousIterative<JacobiProcessor<double>,
 * StopAfterTime, TrivialResiliencePolicy>; Waiter<IterMethod> iter_waiter =
 *  WaiterBuilder<IterMethod>(manager_handle, job, my_tag, nbr_tags)
 *  .set_processor(A, b, row_inds)
 *  .set_iteration_policy(std::chrono::seconds(5))
 *  .set_resilience_policy()
 *  .build_waiter();
 * IterMethod sync_jacobi = iter_waiter.get();
 * @endcode
 */
template <typename Processor, typename IterationPolicy, typename ResiliencePolicy>
class WaiterBuilder<
    SynchronousIterative<Processor, IterationPolicy, ResiliencePolicy>>
{
public:
    using ObjectT =
        SynchronousIterative<Processor, IterationPolicy, ResiliencePolicy>;
    using ThisT = WaiterBuilder<ObjectT>;
    using TagType = typename ObjectT::BaseT::TagType;

    /** @brief WaiterBuilder constructor for SynchronousIterative methods.
     *
     * Note that the user does not directly pass the tags, but passes
     * the string IDs that are used to construct the tags. This is
     * because the actual tag types, which are templated on the data
     * types being sent, can be quite complex depending on the
     * policies used. So, let Skywing worry about building the correct
     * tag type.
     *
     * @param handle ManagerHandle object running this agent.
     * @param job The job running the iteration.
     * @param produced_tag_id The string ID of the tag of the data produced by
     * this agent.
     * @param sub_tag_ids An iteration-capable container of string IDs of tags
     * of neighboring data from which this agent will collect updates.
     */
    template <typename Range>
    WaiterBuilder(ManagerHandle handle,
                  Job& job,
                  const std::string produced_tag_id,
                  const Range& sub_tag_ids)
        : handle_(handle),
          job_(job),
          produced_tag_(produced_tag_id),
          tags_vec_(sub_tag_ids.cbegin(), sub_tag_ids.cend())
    {
        job.declare_publication_intent(produced_tag_);
        subscribe_waiter_ =
            std::make_shared<Waiter<void>>(job.subscribe_range(tags_vec_));
    }

    /** @brief Build a Waiter<Processor> that will construct the Processor for
     * this iterative method.
     */
    template <typename... Args>
    ThisT& set_processor(Args&&... args)
    {
        processor_waiter_ = std::make_shared<Waiter<Processor>>(
            std::move(WaiterBuilder<Processor>(std::forward<Args>(args)...)
                          .build_waiter()));
        return *this;
    }

    /** @brief Build a Waiter<IterationPolicy> that will construct the IterationPolicy for
     * this iterative method.
     */
    template <typename... Args>
    ThisT& set_iteration_policy(Args&&... args)
    {
        iteration_policy_waiter_ = std::make_shared<Waiter<IterationPolicy>>(
            WaiterBuilder<IterationPolicy>(std::forward<Args>(args)...)
                .build_waiter());
        return *this;
    }

    /** @brief Build a Waiter<ResiliencePolicy> that will construct the
     * ResiliencePolicy for this iterative method.
     */
    template <typename... Args>
    ThisT& set_resilience_policy(Args&&... args)
    {
        resilience_policy_waiter_ = std::make_shared<Waiter<ResiliencePolicy>>(
            WaiterBuilder<ResiliencePolicy>(std::forward<Args>(args)...)
                .build_waiter());
        return *this;
    }

    /* @brief Build a Waiter to the desired synchronous iterative method.
     * @returns A Waiter<SynchronousIterative<Processor, IterationPolicy>>
     */
    Waiter<ObjectT> build_waiter()
    {
        if (!(subscribe_waiter_ && processor_waiter_ && iteration_policy_waiter_
              && resilience_policy_waiter_))
            throw std::runtime_error(
                "WaiterBuilder<SynchronousIterative> requires having built all "
                "necessary components prior to calling build_waiter().");

        // capture by value to ensure liveness of shared ptrs
        auto is_ready = [subscribe_waiter_ = this->subscribe_waiter_,
                         processor_waiter = this->processor_waiter_,
                         iteration_policy_waiter = this->iteration_policy_waiter_,
                         resilience_policy_waiter =
                             this->resilience_policy_waiter_]() {
            return (subscribe_waiter_->is_ready()
                    && processor_waiter->is_ready()
                    && iteration_policy_waiter->is_ready()
                    && resilience_policy_waiter->is_ready());
        };

        auto cons_args = std::make_tuple(std::ref(job_),
                                         produced_tag_,
                                         tags_vec_,
                                         processor_waiter_->get(),
                                         iteration_policy_waiter_->get(),
                                         resilience_policy_waiter_->get());
        auto get_object = [cons_args = std::move(cons_args)]() {
            return std::make_from_tuple<ObjectT>(cons_args);
        };
        return handle_.waiter_on_subscription_change<ObjectT>(
            is_ready, std::move(get_object));
    }

private:
    ManagerHandle handle_;
    Job& job_;
    TagType produced_tag_;
    std::vector<TagType> tags_vec_;

    // Using shared_ptrs on these waiters so that they are not destroyed
    // if the WaiterBuilder gets destroyed before the
    // object is retrieved from the Waiter<ThisT>.
    std::shared_ptr<Waiter<void>> subscribe_waiter_;
    std::shared_ptr<Waiter<Processor>> processor_waiter_;
    std::shared_ptr<Waiter<IterationPolicy>> iteration_policy_waiter_;
    std::shared_ptr<Waiter<ResiliencePolicy>> resilience_policy_waiter_;
}; // class WaiterBuilder<...>

} // namespace skywing

#endif // SKYWING_MID_SYNCHRONOUS_ITERATIVE_HPP
