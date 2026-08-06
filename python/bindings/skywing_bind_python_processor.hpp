#ifndef SKYWING_BIND_PYTHON_PROCESSOR_HPP
#define SKYWING_BIND_PYTHON_PROCESSOR_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tuple>
#include <vector>
#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/associative_vector.hpp"
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

  PythonProcessor(py::object py_processor)
    : py_processor_(std::move(py_processor))
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
    py_processor_.attr("process_update")(my_id, py::cast(data));
  }

  ValueType prepare_for_publication(ValueType)
  {
    py::gil_scoped_acquire gil; 
    py::tuple result = py_processor_.attr("prepare_for_publication")();

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
}; // class PythonProcessor


// WM: for some reason, I could not get the ProcessorSyncWrapper to work with PythonProcessor...
// something about PythonProcessor being a regular class instead of a templated class?
// This is basically copy/paste/modify from ProcessorSyncWrapper
class SyncPythonProcessor
    : public PythonProcessor
{
public:
    using ValueType = std::tuple<int, typename PythonProcessor::ValueType, typename PythonProcessor::ValueType>;

    template <typename... UArgs>
    SyncPythonProcessor(UArgs&&... args) :
        PythonProcessor(std::forward<UArgs>(args)...),
        prev_iterate_(PythonProcessor::get_init_publish_values()),
        iteration_count_(0)
    {}

    ValueType get_init_publish_values()
    {
        return ValueType(iteration_count_, prev_iterate_, PythonProcessor::get_init_publish_values());
    }

    template <typename IterMethod>
    void process_update(const DataHandler<ValueType>& wrapper_data_handler,
                        [[maybe_unused]] const IterMethod& iter_method)
    {
        std::unordered_map<std::string, typename PythonProcessor::ValueType> p_handler_update_map;
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
        prev_iterate_ = PythonProcessor::prepare_for_publication(prev_iterate_);
        PythonProcessor::process_update(processor_data_handler_, iter_method);
        iteration_count_++;
    }

    ValueType prepare_for_publication(ValueType vals_to_publish)
    {
        return ValueType(iteration_count_, prev_iterate_, PythonProcessor::prepare_for_publication(std::get<2>(vals_to_publish)));
    }

private:
    typename PythonProcessor::ValueType prev_iterate_;
    DataHandler<typename PythonProcessor::ValueType> processor_data_handler_;
    int iteration_count_;
};
  
} // namespace skywing

#endif // SKYWING_BIND_PYTHON_PROCESSOR_HPP
