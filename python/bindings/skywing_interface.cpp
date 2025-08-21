#include <chrono>
#include <sstream>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// #include "skywing_core/skywing.hpp"
#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_jobs.cpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/big_float.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/python_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include "skywing_mid/sum_processor.hpp"

namespace py = pybind11;
using namespace skywing;

/* Summation jobs */
using CountProcessor = QUACCProcessor<BigFloat,
                                      MinProcessor<BigFloat>,
                                      PushFlowProcessor<BigFloat>>;

template <typename val_t>
using SumMethod = SumProcessor<val_t, PushFlowProcessor<val_t>, CountProcessor>;

template <typename val_t>
using ConsensusSumJob = ConsensusJob<val_t, SumMethod>;

/* Idempotent jobs */
template <typename val_t>
using ConsensusMaxJob = ConsensusJob<val_t, MaxProcessor>;

template <typename val_t>
using ConsensusMinJob = ConsensusJob<val_t, MinProcessor>;

template <typename val_t>
using ConsensusLogicalAndJob = ConsensusJob<val_t, LogicalAndProcessor>;

template <typename val_t>
using ConsensusLogicalOrJob = ConsensusJob<val_t, LogicalOrProcessor>;

/* Bindings */

template <typename val_t>
using StandardVector = skywing::AssociativeVector<std::uint32_t, val_t, true>;

template <typename val_t>
void bind_associative_vector(py::module_& m, const char* name)
{
    py::class_<StandardVector<val_t>>(m, name)
        .def(py::init<>())
        .def("__getitem__",
             &StandardVector<val_t>::operator[])         // Bind the getter
        .def("__setitem__", &StandardVector<val_t>::set) // Bind the setter
        .def("size", &StandardVector<val_t>::size)
        .def("__repr__", [](const StandardVector<val_t>& v) {
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < v.size(); ++i) {
                oss << v.at(i);
                if (i < v.size() - 1) { // Avoid trailing comma
                    oss << ", ";
                }
            }
            oss << "]";
            return oss.str();
        });
}

template <typename val_t>
void bind_ConsensusSumJob(py::module_& m, const char* name)
{
    py::class_<ConsensusSumJob<val_t>>(m, name, py::dynamic_attr())
        .def(py::init<val_t, std::string, std::vector<std::string>&&, size_t>())
        .def("set_value", &ConsensusSumJob<val_t>::set_value)
        .def("get_result", &ConsensusSumJob<val_t>::get_result)
        .def("is_waiter_finished", &ConsensusSumJob<val_t>::is_waiter_finished)
        .def("submit_to_manager", &ConsensusSumJob<val_t>::submit_to_manager);
}

void bind_ConsensusPythonJob(py::module_& m, const char* name)
{
    py::class_<ConsensusPythonJob>(m, name, py::dynamic_attr())
        .def(py::init<py::object,
                      int,
                      std::string,
                      std::vector<std::string>&&,
                      size_t>())
        .def("is_waiter_finished", &ConsensusPythonJob::is_waiter_finished)
        .def("submit_to_manager", &ConsensusPythonJob::submit_to_manager);
}

void bind_Manager(py::module_& m, const char* name)
{
    py::class_<Manager>(m, name)
        .def(py::init<std::uint16_t, std::string&>())
        .def("id", &Manager::id)
        .def("submit_job", &Manager::submit_job)
        .def("run", &Manager::run, py::call_guard<py::gil_scoped_release>())
        .def(
            "configure_initial_neighbors",
            [](Manager& self,
               std::string address,
               std::uint16_t port,
               int timeout = 10) {
                self.configure_initial_neighbors(
                    address, port, std::chrono::seconds(timeout));
            },
            py::arg("address"),
            py::arg("port"),
            py::arg("timeout") = 10);
}

PYBIND11_MODULE(skywing_cpp_interface, m)
{
    bind_associative_vector<double>(m, "DoubleVector");
    bind_associative_vector<int>(m, "IntVector");
    bind_ConsensusSumJob<double>(m, "CollectiveDoubleSum");
    bind_ConsensusSumJob<StandardVector<double>>(m, "CollectiveVectorSum");
    bind_ConsensusPythonJob(m, "ConsensusJob");
    bind_Manager(m, "Manager");
}
