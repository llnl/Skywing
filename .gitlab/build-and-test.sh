#! /usr/bin/env bash

# Basic setup
modules_home=${MODULESHOME:-"/usr/share/lmod/lmod"}
if test -e ${modules_home}/init/bash
then
  . ${modules_home}/init/bash
fi

set -o errexit
set -o nounset

hostname="$(hostname)"
cluster=${hostname//[0-9]/}
project_dir="$(git rev-parse --show-toplevel)"
if [[ $? -eq 1 ]]
then
    project_dir="$(pwd)"
fi

if [[ -n "${MODULES}" ]]
then
    echo "Loading modules: \"${MODULES}\""
    module load ${MODULES}
fi

case "${CXX_COMPILER}" in
    clang++)
        export CC=$(command -v clang)
        export CXX=$(command -v clang++)
        ASAN_FLAGS="-fno-omit-frame-pointer -fsanitize=address"
        ;;
    icpc)
        export CC=$(command -v icc)
        export CXX=$(command -v icpc)
        export CFLAGS="-fp-model=strict"
        export CXXFLAGS="-fp-model=strict"
        ;;
    icpx)
        export CC=$(command -v icx)
        export CXX=$(command -v icpx)
        export CFLAGS="-fp-model=strict"
        export CXXFLAGS="-fp-model=strict"
        ;;
    *)
        export CC=$(command -v gcc)
        export CXX=$(command -v g++)
        ASAN_FLAGS="-fno-omit-frame-pointer -fsanitize=address"
        ;;
esac

echo "----------------------------------------------------------------------"
echo "Building and testing Skywing"
echo "       Cluster: ${cluster}"
echo "      Hostname: ${hostname}"
echo "    C Compiler: ${CC}"
echo "  C++ Compiler: ${CXX}"
echo "----------------------------------------------------------------------"


SOURCE_DIR=${project_dir}
BUILD_DIR=${SOURCE_DIR}/ci-build
INSTALL_DIR=${SOURCE_DIR}/ci-install


echo "----------------------------------------------------------------------"
echo "  Configuring Skywing"
echo "     Source Directory: ${SOURCE_DIR}"
echo "      Build Directory: ${BUILD_DIR}"
echo "    Install Directory: ${INSTALL_DIR}"
echo "----------------------------------------------------------------------"

cd ${SOURCE_DIR}
cmake -G Ninja \
      -S . \
      -B ${BUILD_DIR} \
      \
      -D CMAKE_BUILD_TYPE=Debug \
      -D CMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
      \
      -D SKYWING_DEVELOPER_BUILD=ON \
      -D BUILD_SHARED_LIBS=ON \
      \
      -D SKYWING_BUILD_EXAMPLES=ON \
      -D SKYWING_BUILD_LC_EXAMPLES=ON \
      -D SKYWING_USE_EIGEN=${TEST_EIGEN} \
      -D SKYWING_BUILD_TESTS=ON |& tee ${SOURCE_DIR}/configure-outerr.log

echo "----------------------------------------------------------------------"
echo "  Building Skywing"
echo "----------------------------------------------------------------------"

# Need to set START_PORT so Catch tests can start enough to generate a
# list of tests that will be run.
START_PORT=15005 cmake --build ${BUILD_DIR} |& tee ${SOURCE_DIR}/build-outerr.log

# ugh bash
build_status="${PIPESTATUS[0]}"
if [[ "${build_status}" -ne 0 ]];
then
    echo "FAIL: build failure."
    exit ${build_status}
fi

echo "----------------------------------------------------------------------"
echo "  Testing Skywing"
echo "----------------------------------------------------------------------"

# FIXME (trb 2023/12/05) -- This would be the ideal way to run these
# tests. However, the START_PORT will always be the same, so there
# could potentially be issues getting all the tests to run. I'm
# working on a viable solution here. (Alternatively, if I can get it
# to work with just the Catch2 binary, I'll call that instead.)
# START_PORT=15005 ctest \
#                  --timeout 10 \
#                  --repeat until-pass:3 \
#                  --output-junit ${SOURCE_DIR}/ctest-junit.xml \
#                  --test-dir ${BUILD_DIR}

# Run the tester script.
python3 \
    ${SOURCE_DIR}/scripts/tester_script.py \
    ${BUILD_DIR}/tests/test_runner.txt |& tee ${SOURCE_DIR}/testing-outerr.log

# ugh bash
test_status="${PIPESTATUS[0]}"
if [[ "${test_status}" -ne 0 ]];
then
    echo "FAIL: test failure."
    exit ${test_status}
fi

echo "----------------------------------------------------------------------"
echo "  Installing Skywing"
echo "----------------------------------------------------------------------"

cmake --install ${BUILD_DIR} |& tee ${SOURCE_DIR}/install-outerr.log

# ugh bash
install_status="${PIPESTATUS[0]}"
if [[ "${install_status}" -ne 0 ]];
then
    echo "FAIL: install failure."
    exit ${install_status}
fi

echo "----------------------------------------------------------------------"
echo "  Building tutorial examples"
echo "----------------------------------------------------------------------"

export CMAKE_PREFIX_PATH=${INSTALL_DIR}:${CMAKE_PREFIX_PATH}
if [[ -z "${CXXFLAGS:+x}" ]]
then
    export CXXFLAGS="${ASAN_FLAGS}"
else
    export CXXFLAGS="${CXXFLAGS} ${ASAN_FLAGS}"
fi

EX_BUILD_DIR=${SOURCE_DIR}/build-tutorial-examples
cmake -GNinja \
      -S ${SOURCE_DIR}/documentation/tutorial/example-cmake-project \
      -B ${EX_BUILD_DIR} \
      -D CMAKE_BUILD_TYPE=Release |& tee ${SOURCE_DIR}/tutorial-config-outerr.log

config_status="${PIPESTATUS[0]}"
if [[ "${config_status}" -ne 0 ]];
then
    echo "FAIL: tutorial example configure failure."
    exit ${config_status}
fi

cmake --build ${EX_BUILD_DIR} |& tee ${SOURCE_DIR}/tutorial-build-outerr.log

build_status="${PIPESTATUS[0]}"
if [[ "${build_status}" -ne 0 ]];
then
    echo "FAIL: tutorial example build failure."
    exit ${build_status}
fi

echo "----------------------------------------------------------------------"
echo "  Running tutorial examples"
echo "----------------------------------------------------------------------"

# A modified version of ex4 is tested in tests/skywing_mid/ex4.cpp
# which mirrors documentation/tutorial/example-cmake-project/ex4.cpp
for EXAMPLE in ex1 ex2 ex3
do
    echo "----------------------------------------------------------------------"
    echo "    Running example ${EXAMPLE}"
    echo "----------------------------------------------------------------------"

    ${EX_BUILD_DIR}/run_${EXAMPLE}.sh |& tee ${SOURCE_DIR}/tutorial-${EXAMPLE}-outerr.log
    run_status="${PIPESTATUS[0]}"
    if [[ "${run_status}" -ne 0 ]];
    then
        echo "FAIL: tutorial ${EXAMPLE} run failure."
        exit ${run_status}
    fi
done
