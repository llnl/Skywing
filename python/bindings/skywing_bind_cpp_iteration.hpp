#ifndef SKYWING_BIND_ITERATION_HPP
#define SKYWING_BIND_ITERATION_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/big_float.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include "skywing_mid/sum_processor.hpp"
#include "skywing_mid/synchronous_iterative.hpp"

#include "skywing_bind_python_processor.hpp"

namespace py = pybind11;
using namespace skywing;

////////////////////////////////////////////////////////////////
// Class and binding for iterations using a python processor
////////////////////////////////////////////////////////////////

class CppIteration_PythonProc_Job
{
public:
    CppIteration_PythonProc_Job(py::object py_processor,
				int uid,
				std::string this_id,
				std::vector<std::string>&& sum_sub_tags,
				bool synchronous,
				size_t run_duration_secs)
        : py_processor_(std::move(py_processor)),
          uid_(uid),
          this_id_(this_id),
          sum_sub_tags_(std::move(sum_sub_tags)),
          synchronous_(synchronous),
          run_duration_secs_(run_duration_secs)
    {}

    bool is_waiter_finished() const { return waiter_finished_; }

    void submit_to_manager(Manager& manager, std::string& job_name)
    {
        // Submit a synchronous iterative method job
        if (synchronous_) {
            auto iteration_job_fun = [&](skywing::Job& job,
                                         skywing::ManagerHandle manager_handle) {
                using SyncIterMethod = SynchronousIterative<SyncPythonProcessor,
                                                            IterateUntilTime,
                                                            TrivialResiliencePolicy>;

                for (auto& t : sum_sub_tags_) {
                    std::cout << t << " ";
                }
                std::cout << std::endl;
                Waiter<SyncIterMethod> iter_waiter =
                    WaiterBuilder<SyncIterMethod>(
                        manager_handle, job, this_id_, sum_sub_tags_)
                        .set_processor(py_processor_)
                        .set_iteration_policy(
                            std::chrono::seconds(run_duration_secs_))
                        .set_resilience_policy()
                        .build_waiter();
                SyncIterMethod iteration = iter_waiter.get();
                waiter_finished_ = true;

                auto update_fun = [&](SyncIterMethod& p) {
                    (void) p;
                    return;
                };

                std::cout << "Running iteration..." << std::endl;
                iteration.run(update_fun);
            };
            manager.submit_job(job_name, iteration_job_fun);
        }
        else {
            auto iteration_job_fun = [&](skywing::Job& job,
                                         skywing::ManagerHandle manager_handle) {
                using AsyncIterMethod = AsynchronousIterative<PythonProcessor,
                                                              AlwaysPublish,
                                                              IterateUntilTime,
                                                              TrivialResiliencePolicy>;

                for (auto& t : sum_sub_tags_) {
                    std::cout << t << " ";
                }
                std::cout << std::endl;
                Waiter<AsyncIterMethod> iter_waiter =
                    WaiterBuilder<AsyncIterMethod>(
                        manager_handle, job, this_id_, sum_sub_tags_)
                        .set_processor(py_processor_)
                        .set_publish_policy()
                        .set_iteration_policy(
                            std::chrono::seconds(run_duration_secs_))
                        .set_resilience_policy()
                        .build_waiter();
                AsyncIterMethod iteration = iter_waiter.get();
                waiter_finished_ = true;

                auto update_fun = [&](AsyncIterMethod& p) {
                    (void) p;
                    return;
                };

                std::cout << "Running iteration..." << std::endl;
                iteration.run(update_fun);
		std::cout << "Finished iteration." << std::endl;
            };
            manager.submit_job(job_name, iteration_job_fun);
        }
    }

private:
    py::object py_processor_;
    int uid_;

    std::string this_id_;
    std::vector<std::string> sum_sub_tags_;
    bool synchronous_;
    size_t run_duration_secs_;
    bool waiter_finished_ = false;
}; // class CppIteration_PythonProc_Job

void bind_CppIteration_PythonProc(py::module_& m, const char* name)
{
    py::class_<CppIteration_PythonProc_Job>(m, name, py::dynamic_attr())
        .def(py::init<py::object,
                      int,
                      std::string,
                      std::vector<std::string>&&,
                      bool,
                      size_t>())
        .def("is_waiter_finished", &CppIteration_PythonProc_Job::is_waiter_finished)
        .def("submit_to_manager", &CppIteration_PythonProc_Job::submit_to_manager);
}


////////////////////////////////////////////////////////////////
// Class and bindings for iterations using C++ processors
////////////////////////////////////////////////////////////////

template <typename val_t, template<typename...> class processor_t, typename... Args>
class CppIteration_CppProc_Job
{
public:
    CppIteration_CppProc_Job(val_t data,
			     std::string this_id,
			     std::vector<std::string>&& sum_sub_tags,
			     bool synchronous,
			     size_t run_duration_secs)
        : data_(data),
          result_(data),
          this_id_(this_id),
          sum_sub_tags_(std::move(sum_sub_tags)),
          synchronous_(synchronous),
          run_duration_secs_(run_duration_secs)
    {}

    void update_data(val_t data) { data_ = data; }

    val_t get_result() { return result_; }

    bool is_waiter_finished() { return waiter_finished_; }

    void submit_to_manager(Manager& manager, std::string& job_name)
    {
        if (synchronous_) {
            auto job_fun = [&](skywing::Job& job,
                               skywing::ManagerHandle manager_handle) {
                using SyncIterMethod = SynchronousIterative<ProcessorSyncWrapper<processor_t, Args...>,
                                                            IterateUntilTime,
                                                            TrivialResiliencePolicy>;

                Waiter<SyncIterMethod> iter_waiter =
                    WaiterBuilder<SyncIterMethod>(
                        manager_handle, job, this_id_, sum_sub_tags_)
                        // WM: todo - we are assuming a particular form of the processor constructor, which is not universal...
                        .set_processor(data_)
                        .set_iteration_policy(
                            std::chrono::seconds(run_duration_secs_))
                        .set_resilience_policy()
                        .build_waiter();
                SyncIterMethod iteration = iter_waiter.get();
                waiter_finished_ = true;

                auto update_fun = [&](SyncIterMethod& p) {
                    result_ = p.get_processor().get_value();
                    p.get_processor().set_value(data_);
                };

                iteration.run(update_fun);
            };
            manager.submit_job(job_name, job_fun);
        }
        else {
            auto job_fun = [&](skywing::Job& job,
                               skywing::ManagerHandle manager_handle) {
                using AsyncIterMethod = AsynchronousIterative<processor_t<Args...>,
                                                              AlwaysPublish,
                                                              IterateUntilTime,
                                                              TrivialResiliencePolicy>;

                Waiter<AsyncIterMethod> iter_waiter =
                    WaiterBuilder<AsyncIterMethod>(
                        manager_handle, job, this_id_, sum_sub_tags_)
                        // WM: todo - we are assuming a particular form of the processor constructor, which is not universal...
                        .set_processor(data_)
                        .set_publish_policy()
                        .set_iteration_policy(
                            std::chrono::seconds(run_duration_secs_))
                        .set_resilience_policy()
                        .build_waiter();
                AsyncIterMethod iteration = iter_waiter.get();
                waiter_finished_ = true;

                auto update_fun = [&](AsyncIterMethod& p) {
                    result_ = p.get_processor().get_value();
                    p.get_processor().set_value(data_);
                };

                iteration.run(update_fun);
            };
            manager.submit_job(job_name, job_fun);
        }
    }

private:
    val_t data_;
    val_t result_;
    std::string this_id_;
    std::vector<std::string> sum_sub_tags_;
    bool synchronous_;
    size_t run_duration_secs_;
    bool waiter_finished_ = false;
}; // class CppIteration_CppProc_Job

// Generic, templated binding for all C++ iterations
template <typename val_t, template<typename...> class processor_t, typename... Args>
void bind_CppIteration_CppProc(py::module_& m, const char* name)
{
    py::class_<CppIteration_CppProc_Job<val_t, processor_t, Args...>>(m, name, py::dynamic_attr())
        .def(py::init<val_t, std::string, std::vector<std::string>&&, bool, size_t>())
        .def("update_data", &CppIteration_CppProc_Job<val_t, processor_t, Args...>::update_data)
        .def("get_result", &CppIteration_CppProc_Job<val_t, processor_t, Args...>::get_result)
        .def("is_waiter_finished", &CppIteration_CppProc_Job<val_t, processor_t, Args...>::is_waiter_finished)
        .def("submit_to_manager", &CppIteration_CppProc_Job<val_t, processor_t, Args...>::submit_to_manager);
}

// Templated binding for sum iterations
template <typename val_t>
void bind_CppIterationSum(py::module_& m, const char* name)
{
    using CountProcessor = QUACCProcessor<BigFloat,
                                          MinProcessor<BigFloat>,
                                          PushFlowProcessor<BigFloat>>;
    using SumMethod = SumProcessor<val_t,
                                   PushFlowProcessor<val_t>,
                                   CountProcessor>;
    bind_CppIteration_CppProc<val_t, SumProcessor, val_t, PushFlowProcessor<val_t>, CountProcessor>(m, name);
}

// WM: todo - Additional templated bindings for more C++ processors

#endif // SKYWING_BIND_ITERATION_HPP
