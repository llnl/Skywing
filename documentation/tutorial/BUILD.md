@defgroup build-tutorial Building Tutorials

Building the examples from the tutorial is a straightforward CMake
project. The Skywing library must be built **and installed** prior to
building the example executables.

Once Skywing has been built and installed, the tutorials can be built
as any other CMake project. For the following, suppose that Skywing
has been installed to `${SKYWING_PREFIX}` and if you are acquiring the dependency yourself, CapnProto has been
installed to `${CAPNPROTO_PREFIX}`. The directory in which this
`BUILD.md` file is located will be `${TUTORIAL_HOME}`.

```
# If Skywing is installed to a nonstandard location, e.g.,
# to ${SKYWING_PREFIX}, update the CMAKE_PREFIX_PATH:
export CMAKE_PREFIX_PATH=${SKYWING_PREFIX}:${CMAKE_PREFIX_PATH}

# Similarly, if CapnProto is installed to a nonstandard location,
# e.g., to ${CAPNPROTO_PREFIX}, update the CMAKE_PREFIX_PATH:
export CMAKE_PREFIX_PATH=${CAPNPROTO_PREFIX}:${CMAKE_PREFIX_PATH}

# Configure the project
cmake -S ${TUTORIAL_HOME}/example-cmake-project \
      -B ${TUTORIAL_HOME}/build-tutorial \
      -D CMAKE_BUILD_TYPE=Release

# Build the project
cmake --build ${TUTORIAL_HOME}/build-tutorial
```

This will build the three tutorial executables: `ex1`, `ex2`, and
`ex3`. It will also generate a shell script to run each one,
`run_ex{1,2,3}.sh`. See the tutorial files for more information on the
content and intent of each of these executables.
