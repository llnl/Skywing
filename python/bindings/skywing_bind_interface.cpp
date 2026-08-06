#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "skywing_bind_cpp_iteration.hpp"
#include "skywing_bind_manager.hpp"
#include "skywing_bind_job.hpp"
#include "skywing_bind_python_processor.hpp"

#include "skywing_mid/associative_vector.hpp"

using StandardVector = skywing::AssociativeVector<std::uint32_t, double, true>;

namespace py = pybind11;
using namespace skywing;

PYBIND11_MODULE(skywing_bind_interface, m)
{
    // Manager
    bind_Manager(m, "Manager");
    bind_Job(m, "JobHandle");

    // Iterations
    // PORT: This will eventually be removed in favor of a pure python implementaiton of Iteration, which
    //       will replace calls to BindIteration in the current Iteration class (core/iteration.py).
    bind_CppIteration_PythonProc(m, "CppIteration_PythonProc");
    bind_CppIterationSum<double>(m, "CppIterationSumScalar");
    bind_CppIterationSum<StandardVector>(m, "CppIterationSumVector");
}
