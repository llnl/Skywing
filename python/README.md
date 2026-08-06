This module defines a set of Python objects that can be used to
build Python-based Skywing software.

To install this,
* (Recommended) Create a Python virtual environment and activate it.
* From the `Skywing` folder, run `pip install .`

Extra control is possible, e.g.,

    CMAKE_ARGS="-DSKYWING_BUILD_TESTS=ON -DSKYWING_BUILD_EXAMPLES=ON -DSKYWING_DEVELOPER_BUILD=ON DSKYWING_WARNINGS_AS_ERRORS=ON -DSKYWING_ENABLE_MEMCHECK=OFF -DSKYWING_LOG_LEVEL=trace DSKYWING_USE_EIGEN=ON" pip install .


The CMake build will happen in $PWD/skbuild.

Editable installs are recommended to be run with --no-build-isolation, in which case the user is responsible for providing all of the required dependencies.

# Main classes

A few key classes are defined:

* Agent: Define a local address and neighboring connections
that will be used for communication during an Iteration.

* Iteration: An iteration object defines a particular
iterative algorithm that can be used in building Python executables.
Iterations must be provided with an Agent object describing the
local and neighboring addresses for communication and a Processor
describing the local computation and communication data handling.
Iterations begin computing and communicating when the `launch()` function
is called, and will run continuously in their own thread until a user-
defined stopping criteria (e.g. `max_time`) is reached. The user may
update data used by the iteration via the `update_data()` function
or get a current result from the iteration via `query()` (note these
functions may be called while the iteraiton is running and will not
interrupt execution).

PORT:
We continue to support some cross-compatibility with the legacy C++
implementation. The C++ implementation of `Iteration` may be used in
conjunction with Python processors via `CppIteration_PythonProc`,
which takes the class type of the processor (not the instantiated
object) as an argument and an `AgentCpp` object for the Skywing agent.
See `drivers/utils/ls_driver_subprocess.py` for an example.
Additionally, C++ processors may be used in conjunction with the
C++ iteration implementation via `CppIteration_CppProc` used with
specific pybind declarations of iterations with C++ processors, such
as `CppIterationSumScalar`. See `bindings/skywing_bind_interface.py`.
The Python implementation of `Iteration` is also compatible with the
C++ implementation of the core via `IterationCppCore` which takes
a Python processor object and an `AgentCpp` object as the Skywing
agent. To summarize:
1. C++ iteration with Python processor: `CppIteration_PythonProc`
2. C++ iteration with C++ processor: `CppIteration_CppProc`
3. Python iteration with Python processor and C++ core: `IterationCppCore`, `AgentCpp`
