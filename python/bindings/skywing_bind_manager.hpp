#ifndef SKYWING_BIND_MANAGER_HPP
#define SKYWING_BIND_MANAGER_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "skywing_core/manager.hpp"

namespace py = pybind11;
using namespace skywing;

#define PYBIND11_DETAILED_ERROR_MESSAGES

void bind_Manager(py::module_& m, const char* name)
{
    py::class_<Manager>(m, name)
        .def(py::init<std::uint16_t, std::string&>())
        .def("id", &Manager::id)
        .def("submit_job", &Manager::submit_job)
        .def("run", &Manager::run, py::call_guard<py::gil_scoped_release>())
        // WM: todo - If timeout is reached, then it seems nothing happens...? Jobs submitted to the manager just don't run.
        //            Should provide a warning/error? Or allow jobs to proceed with just local info?
        .def(
            "configure_initial_neighbors",
            [](Manager& self,
               std::string address,
               std::uint16_t port,
               int timeout) {
                self.configure_initial_neighbors(
                    address, port, std::chrono::seconds(timeout));
            },
            py::arg("address"),
            py::arg("port"),
            py::arg("timeout") = 10)
      .def(
	 "submit_pyjob",
	 [](Manager& self,
	    std::string job_name,
	    py::object py_job) {
	   auto py_job_fun = [py_job](skywing::Job& job_handle,
				 skywing::ManagerHandle manager_handle) {
	     py::gil_scoped_acquire gil;
	     py_job.attr("run")(std::ref(job_handle));
	   };
	   self.submit_job(job_name, py_job_fun);
	 },
	 py::arg("job_name"),
	 py::arg("py_job"));
}

#endif // SKYWING_BIND_MANAGER_HPP
