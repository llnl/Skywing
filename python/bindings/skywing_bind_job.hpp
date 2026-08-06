#ifndef SKYWING_BIND_JOB_HPP
#define SKYWING_BIND_JOB_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "skywing_core/tag.hpp"
#include "skywing_core/job.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"

namespace py = pybind11;
using namespace skywing;

void bind_Job(py::module_& m, const char* name)
{
  using ValueType = std::tuple<
    std::vector<std::string>,
    std::vector<double>,
    std::vector<int>
    >;
  using TagType = UnwrapAndApply_t<ValueType, Tag>;
  
    py::class_<Job>(m, name)
      .def("declare_publication_intent",
	   [](Job& self,
	      std::string tag_id) {
	     self.declare_publication_intent(TagType(tag_id));
	   })
      .def("publish",
	   [](Job& self,
	      std::string tag_id,
	      py::tuple value) {
	     if (value.size() != 3) {
	       throw std::runtime_error("Job::publish python interace: Expected tuple of 3 lists");
	     }

	     // Convert each list into a std::vector
	     std::vector<std::string> val_s = value[0].cast<std::vector<std::string>>();
	     std::vector<double> val_d = value[1].cast<std::vector<double>>();
	     std::vector<int> val_i = value[2].cast<std::vector<int>>();
    
	     self.publish(TagType(tag_id), val_s, val_d, val_i);
	   })
      .def("subscribe",
	   [](Job& self,
	      std::string tag_id) {
	     self.subscribe(TagType(tag_id));
	   })
      .def("has_data",
	   [](Job& self,
	      std::string tag_id) {
	     return self.has_data(TagType(tag_id));
	   })
      .def("get_data_if_present",
	   [](Job& self,
	      std::string tag_id) {
	     return self.get_data_if_present(TagType(tag_id));
	   });
}

#endif // SKYWING_BIND_JOB_HPP
