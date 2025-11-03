#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/big_float.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/python_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include "skywing_mid/sum_processor.hpp"

namespace py = pybind11;
using namespace skywing;

template <typename val_t, template <typename> typename ProcessorTemp>
class ConsensusJob
{
    using ProcessorT = ProcessorTemp<val_t>;

public:
    ConsensusJob(val_t start_val,
                 std::string this_id,
                 std::vector<std::string>&& sum_sub_tags,
                 size_t run_duration_secs)
        : input_value_(start_val),
          result_(start_val),
          this_id_(this_id),
          sum_sub_tags_(std::move(sum_sub_tags)),
          run_duration_secs_(run_duration_secs)
    {}

    void set_value(val_t value) { input_value_ = value; }

    val_t get_result() { return result_; }

    bool is_waiter_finished() { return waiter_finished_; }

    void submit_to_manager(Manager& manager, std::string& job_name)
    {
        auto job_fun = [&](skywing::Job& job,
                           skywing::ManagerHandle manager_handle) {
            using IterMethod = AsynchronousIterative<ProcessorT,
                                                     AlwaysPublish,
                                                     IterateUntilTime,
                                                     TrivialResiliencePolicy>;

            Waiter<IterMethod> iter_waiter =
                WaiterBuilder<IterMethod>(
                    manager_handle, job, this_id_, sum_sub_tags_)
                    .set_processor(input_value_)
                    .set_publish_policy()
                    .set_iteration_policy(
                        std::chrono::seconds(run_duration_secs_))
                    .set_resilience_policy()
                    .build_waiter();
            IterMethod iteration = iter_waiter.get();
            waiter_finished_ = true;

            auto update_fun = [&](IterMethod& p) {
                result_ = p.get_processor().get_value();
                p.get_processor().set_value(input_value_);
            };

            iteration.run(update_fun);
        };
        manager.submit_job(job_name, job_fun);
    }

private:
    val_t input_value_;
    val_t result_;
    std::string this_id_;
    std::vector<std::string> sum_sub_tags_;
    size_t run_duration_secs_;
    bool waiter_finished_ = false;
}; // class ConsensusJob

class ConsensusPythonJob
{
public:
    using result_t = std::
        tuple<std::vector<std::string>, std::vector<double>, std::vector<int>>;

    ConsensusPythonJob(py::object py_processor,
                       int uid,
                       std::string this_id,
                       std::vector<std::string>&& sum_sub_tags,
                       size_t run_duration_secs)
        : py_processor_(std::move(py_processor)),
          uid_(uid),
          this_id_(this_id),
          sum_sub_tags_(std::move(sum_sub_tags)),
          run_duration_secs_(run_duration_secs)
    {}

    bool is_waiter_finished() const { return waiter_finished_; }

    void submit_to_manager(Manager& manager, std::string& job_name)
    {
        auto consensus_job_fun = [&](skywing::Job& job,
                                     skywing::ManagerHandle manager_handle) {
            using IterMethod = AsynchronousIterative<PythonProcessor,
                                                     AlwaysPublish,
                                                     IterateUntilTime,
                                                     TrivialResiliencePolicy>;

            for (auto& t : sum_sub_tags_) {
                std::cout << t << " ";
            }
            std::cout << std::endl;
            Waiter<IterMethod> iter_waiter =
                WaiterBuilder<IterMethod>(
                    manager_handle, job, this_id_, sum_sub_tags_)
                    .set_processor(py_processor_, uid_)
                    .set_publish_policy()
                    .set_iteration_policy(
                        std::chrono::seconds(run_duration_secs_))
                    .set_resilience_policy()
                    .build_waiter();
            IterMethod iteration = iter_waiter.get();
            waiter_finished_ = true;

            auto update_fun = [&](IterMethod& p) {
                (void) p;
                return;
            };

            std::cout << "Running iteration..." << std::endl;
            iteration.run(update_fun);
        };
        manager.submit_job(job_name, consensus_job_fun);
    }

private:
    py::object py_processor_;
    int uid_;

    std::string this_id_;
    std::vector<std::string> sum_sub_tags_;
    size_t run_duration_secs_;
    bool waiter_finished_ = false;
}; // class ConsensusPythonJob
