#ifndef ITERATION_CRITERION_HPP
#define ITERATION_CRITERION_HPP

#include <chrono>

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

namespace skywing
{

/* This file contains a number of common iterative methods IterationPolicy
   (stopping criteria) options.
 */

/** @brief IterationPolicy that stops after a given amount of time has
    passed.
 */
class IterateUntilTime
{
public:
    template <typename Duration>
    IterateUntilTime(Duration d) : max_run_time_(d)
    {}

    template <typename CallerT>
    bool init(const CallerT& caller)
    {
        (void) caller;
        return true;
    }

    template <typename CallerT>
    bool operator()(const CallerT& caller)
    {
        return caller.run_time() <= max_run_time_;
    }

private:
    std::chrono::milliseconds max_run_time_;

}; // class IterateUntilTime


using pubtag_t = skywing::Tag<int>;

/**
 * @brief IterationPolicy used by SynchronizingRootNode to control agent iteration.
 *
 * This policy "synchronizes" the agents using a root node. In the init() function,
 * it subscribes to the root node's signal and declares publication intent to a tag based on the Job ID.
 *
 * During execution (operator()), the agent stops and waits for a signal from the root node before continuing.
 * This creates a type of barrier to ensure all agents are on the same iteration.
 *
 */
class WaitForRootSignal
{
public:
    /**
     * @brief Sets up the policy with a given duration (not used in this policy).
     * @param d Duration (unused in this implementation).
     */
    template <typename Duration>
    WaitForRootSignal(Duration d): max_run_time_(d)
    {
    }
    
    /** @brief Configures the policy with initization information, called once before iteration
     * begins in AsynchronousIterative.
     * 
     * In this case, the agent_tag is set based on the job ID, publication intent is declared for that 
     * agent_tag, and the root tag which is hardcoded as "root_signal" is subscribed to. The root node must
     * publish under "root_signal" as well.
    */
    template <typename CallerT>
    bool init(const CallerT& caller) {
        // Construct the agent_tag based on the Job ID to distinguish the agents.
        // This requires the Job IDs to be distinct.
        agent_tag_ = pubtag_t{caller.get_job().id()};
        //std::cout << "Agent tag is " << caller.get_job().id() << std::endl;

        std::cout << "Declaring publication intent for this agent tag" << std::endl;
        caller.get_job().declare_publication_intent(agent_tag_);

        std::cout << "Subscribing to root tag" << std::endl;
        caller.get_job().subscribe(root_tag_).wait();

        // Once initial setup is complete, synchronize the agents before iteration start
        return publish_and_wait(caller);
    }


    template <typename CallerT>
    bool operator()(const CallerT& caller) {
        return publish_and_wait(caller) && (caller.run_time() <= max_run_time_);
    }

    private:
    /**
     * @brief Publishes the agent's current iteration and waits for the root node's signal.
     *
     * @param caller The calling object, for example an AsynchronousIterative method. 
     * @return true when it has recieed the next interation count's continue signal from the root node
     */
    template <typename CallerT>
    bool publish_and_wait(const CallerT& caller) {
        iter_count_ = caller.get_iteration_count();
        if (iter_count_ >= last_sent_count_) {
            bool collective_ready = false;
            while (!collective_ready) {
                //std::cout << "Agent is ready to begin interation " << iter_count_ << std::endl;
                caller.get_job().publish(agent_tag_, iter_count_);
                //std::cout << "Waiting to recieve signal to continue iteration " << iter_count_ << " from root" << std::endl;
                collective_ready = caller.get_job().get_target_val_waiter(root_tag_, iter_count_).wait_for(std::chrono::milliseconds(50));
                // To prevent deadlock, check for timeout here
                if (caller.run_time() > max_run_time_){
                    return false;
                }
            }
            //std::cout << "Recieved signal to continue iteration" << iter_count_ << std::endl;
            last_sent_count_ = iter_count_;
        }
        return true;
    }

    int iter_count_ = -1;
    int last_sent_count_ = 0;
    pubtag_t root_tag_ = pubtag_t{"root_signal"};
    // Default value if init() is not called to overwrite the default
    pubtag_t agent_tag_ = pubtag_t{"agent_signal"};
    std::chrono::milliseconds max_run_time_;
}; 

// template<typename LocalStopPolicy>
// class SynchronousConsensusStop
// {
//  public:
//   using ValueType = bool;

//   SynchronousConsensusStop(size_t collective_graph_diameter_upperbound)
//     : diam_ub_(collective_graph_diameter_upperbound)
//   {}

//   template<typename CallerT>
//   bool operator()(const CallerT& caller)
//   { return iterations_since_all_ready_ > diam_ub_;  }

//   ValueType get_init_publish_values()
//   { return locally_ready_to_stop_; }

//   template<typename NbrDataHandler, typename IterMethod>
//   void process_update(const NbrDataHanlder& nbr_data_handler,
//                       const IterMethod& caller)
//   {
//     locally_ready_to_stop_ = local_stop_policy_(caller);

//     bool is_all_ready = locally_ready_to_stop_ &&
//       nbr_data_handler.f_accumulate<bool>([](const bool& b) {return b;},
//                                           std::logical_and<bool>);
//     if (is_all_ready)
//       ++iterations_since_all_ready_;
//     else
//       iterations_since_all_ready = 0;
//   }

//   bool prepare_for_publication(ValueType)
//   {
//     return iterations_since_all_ready > diam_ub_;
//   }

// private:
//   bool locally_ready_to_stop_ = false;
//   size_t iterations_since_all_ready_ = 0;
//   size_t diam_ub_;

//   LocalStopPolicy local_stop_policy_;
// } // class SynchronousConsensusStop

} // namespace skywing

#endif
