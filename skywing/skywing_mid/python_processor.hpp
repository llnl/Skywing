#ifndef PYTHON_PROCESSOR_HPP
#define PYTHON_PROCESSOR_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tuple>
#include <vector>
#include "skywing_mid/data_handler.hpp"

namespace skywing
{
namespace py = pybind11;
  
class PythonProcessor
{
public:
  using ValueType = std::tuple<
    std::vector<std::string>,
    std::vector<double>,
    std::vector<int>
    >;

  PythonProcessor(py::object py_processor, int uid)
    : py_processor_(std::move(py_processor)),
      uid_(uid)
  {
    py::gil_scoped_acquire gil; 
    
    if (!py::hasattr(py_processor_, "process_update")) {
      throw std::runtime_error("PythonProcessor: py_processor must have a `process_update` attribute.");
    }

    if (!py::hasattr(py_processor_, "prepare_for_publication")) {
      throw std::runtime_error("PythonProcessor: py_processor must have a `prepare_for_publication` attribute.");
    }
  }

  ValueType get_init_publish_values()
  { return {{}, {}, {}}; }

  template<typename IterMethod>
  void process_update(const DataHandler<ValueType>& data_handler,
		      const IterMethod& iter_method)
  {
    std::string my_id = iter_method.my_tag().id();
    std::unordered_map<std::string, ValueType> data;
    for (const auto& pTag : data_handler.recvd_data_tags()) {
      const ValueType& nbr_data = data_handler.get_data(pTag);
      data.try_emplace(pTag, nbr_data);
    }

    py::gil_scoped_acquire gil; 
    py_processor_.attr("process_update")(uid_, my_id, py::cast(data));
  }

  ValueType prepare_for_publication(ValueType)
  {
    py::gil_scoped_acquire gil; 
    py::tuple result = py_processor_.attr("prepare_for_publication")(uid_);

    if (result.size() != 3) {
        throw std::runtime_error("PythonProcessor::prepare_for_publication: Expected tuple of 3 lists");
    }

    // Convert each list into a std::vector
    std::vector<std::string> val_s = result[0].cast<std::vector<std::string>>();
    std::vector<double> val_d = result[1].cast<std::vector<double>>();
    std::vector<int> val_i = result[2].cast<std::vector<int>>();
    
    return std::make_tuple(val_s, val_d, val_i);
  }

private:
  py::object py_processor_;
  int uid_;
}; // class PythonProcessor
  
} // namespace skywing

#endif // PYTHON_PROCESSOR_HPP
