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

* Agent: This class is the primary Python entry point to
Skywing. Through an Agent, one can specify a set of neighbors, and can
pass it a "task", a `__call__` function that will be executed
continuously. This `to_execute` function defines a set of code to be
run in a loop. That code can make various calls to Skywing consensus
objects; each of these spins off a new Skywing "Job" to be run in
parallel.

* Consensus objects: A consensus object defines a particular consensus
gossip algorithm that can be used in building Python
executables. There are two main types of Consensus objects:
  1. CppConsensusOp, a consensus operation whose processor is defined in C++, and
  2.  PythonConsensusOp, a consensus operation whose processor is defined in Python, but which still leverages the underlying C++ gossip framework.

To define a new consensus operation that can be used in a Python
executable, one must simply write
`consensus_op_name = CppConsensusOp(processor_name)`
or
`consensus_op_name = PythonConsensusOp(processor_name)`.

If definining a C++ consensus operation, the `processor_name` must
be an object made available through pybind11 in `skywing_cpp_interface`.
If defining a Python consensus operation, the `processor_name` must be an object
