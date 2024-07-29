```
           _|                                  _|
   _|_|_|  _|  _|    _|    _|  _|          _|      _|_|_|      _|_|
 _|_|      _|_|      _|    _|  _|    _|    _|  _|  _|    _|  _|    _|
     _|_|  _|  _|    _|    _|  _|  _|  _|  _|  _|  _|    _|  _|    _|
 _|_|_|    _|    _|    _|_|_|    _|      _|    _|  _|    _|    _|_|_|
                           _|                                      _|
                       _|_|                                    _|_|
```

A high-reliability, real-time, decentralized platform for
collaborative autonomy. Tutorials for using Skywing can be found in
`documentation/tutorials`. To first build Skywing, follow the
instructions below.

# Building Skywing

Some dependencies are managed by Skywing's build process, and some you
need to acquire yourself beforehand.

## Dependencies Not Automatically Managed
 * compiler that supports C++20 library
   * tested: GCC/g++ (12.1.1) and LLVM/clang (14.0.6)
 * CMake (https://cmake.org/)
 * Cap'n Proto (https://capnproto.org/)
   * requires version 1.0 or newer

## Dependencies Managed as Git Submodules

   You do not need to acquire these yourself.

 * Catch2
   * requires version 3.0 or newer
 * spdlog
   * requires version 1.0 or newer

## Build instructions

Follow these instructions to build a "barebones" version of Skywing
without any tests or examples. If any dependencies have been
installed to nonstandard locations, remember to add the appropriate
paths to `CMAKE_PREFIX_PATH` and/or `PKG_CONFIG_PATH`. E.g., for
CapnProto and a Bourne-like shell, `export
CMAKE_PREFIX_PATH=/path/to/capnproto/install:${CMAKE_PREFIX_PATH}`
(prepending ensures your version will be found before any system install).

 * Build non-managed dependencies separately
 * Get dependencies
   * `git submodule update --init`
 * Configure the project
   * `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKYWING_BUILD_TESTS=ON`
 * Build the project
   * `cmake --build build`
 * Run tests
   * `python3 scripts/tester_script.py build/tests/test_runner.txt`
 * Install the project. This is required for interacting with Skywing
   downstream. Note that the default installation prefix is
   `/usr/local` but it can be changed by passing any desired prefix to
   CMake with `cmake ... -DCMAKE_INSTALL_PREFIX=/path/to/prefix ...`
   * `cmake --install build`

To use Skywing as a library in a downstream application, it is
recommended that it be installed to simplify the build process.

## Enabling Tests and Examples

Tests will be built if `SKYWING_BUILD_TESTS` is enabled, and examples
will be built if `SKYWING_BUILD_EXAMPLES` is enabled in the CMake
configuration. For example, using the CMake CLI:

`cmake -DSKYWING_BUILD_TESTS=ON -DSKYWING_BUILD_EXAMPLES=ON`

## Guidance for building on LC

If you are running on LLNL's LC clusters, these instructions can help you get set up.

### Building capnp
 * Cap'n Proto must be manually built first. Follow the instructions at https://capnproto.org/install.html#installation-unix, except you must install to a local directory. To do this, on the configure step, use
   * `./configure --prefix=/path/to/capnp-prefix && make install`
   * This will create subdirectories `/path/to/capnp-prefix/{bin,include,lib}`

### Building Skywing
 * Load a more recent CMake and switch to more recent version of gcc
   * `ml cmake/3.26.3`
   * `ml gcc/12.1.1-magic`
 * Add capnp prefix directory to `CMAKE_PREFIX_PATH`
   * `export CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH:/path/to/capnp-prefix`
 * Follow build instructions as normal
 * To build the LC Hello World example, also include `-DSKYWING_BUILD_LC_EXAMPLES=ON` in the CMake options.
 * To run the LC example, go to `(skywing_root)/build/examples/lc_hello_world/` and execute `source run.sh (bank_name)`. Note that you must have an active bank to run this test.

### Running Skywing
 * If there are issues finding CapnProto libraries at runtime, please
   file a bug with the development team. In the meantime, adding the
   appropriate path to `LD_LIBRARY_PATH` can often get you moving:
   * `export LD_LIBRARY_PATH=/path/to/capnp-prefix/lib64:${LD_LIBRARY_PATH}`
 * Run as normal for running on login node. Note: can't run long jobs on login nodes!
 * Skywing configurations that involve many connections between agents can run into a file descriptor limit.  The soft limit can be increased by executing `ulimit -n <N>` where `<N>` must not exceed the hard limit (which is determined by executing `ulimit -Hn`)

Note that Skywing configurations that involve many connections between agents can run into a file descriptor limit.
The soft limit can be increased by executing `ulimit -n <N>` where `<N>` must not exceed the hard limit (which can be determined by executing `ulimit -Hn`)

# Building an application on Skywing

As noted in [the build instructions](#build-instructions), the
preferred way to interact with Skywing in downstream applications is
by using the installed artifacts. By default, Skywing will be
installed to `/usr/local`, but the prefix may be altered by passing a
custom value for `CMAKE_INSTALL_PREFIX` during configuration. This has
many benefits, such as unifying include paths and exposing a CMake
export.

## CMake integration

Skywing ships a CMake export in its install artifacts. This can be
used to detect and interact with Skywing in downstream CMake-based
applications and is the preferred way for downstreams to detect
Skywing. The CMake export will capture all the usage requirements for
the available installation of Skywing more precisely than a user might
be able to guess or even detect using another format (such as
pkg-config, which Skywing does not export).

Integration with an existing CMake project is trivial. For example:

```cmake
find_package(Skywing REQUIRED)
target_link_libraries(MyApp PRIVATE skywing::skywing)
```

This should fully capture any dependencies for Skywing. Users should
be aware that the CMake configuration of projects that use the Skywing
export will need to be aware of any Skywing dependencies' locations on
disk. For example, if CapnProto is installed to a nonstandard
location, CMake must be told via any of the standard CMake mechanisms,
such as `CMAKE_PREFIX_PATH=${capnp_prefix}:${CMAKE_PREFIX_PATH}`. See
the [CMake `find_package()`
documentation](https://cmake.org/cmake/help/git-stage/command/find_package.html#config-mode-search-procedure)
for a detailed description of the way CMake searches for dependencies.

An example `CMakeLists.txt` file for building one of the Tutorial
executables is available at
`documentation/tutorial/example-cmake-project`.

# Contributing to Skywing

Skywing is an open source project. We welcome contributions via pull
requests as well as questions, feature requests, or bug reports via
issues. Contact any of our team members with any questions. Please
also refer to our [code of conduct](CODE_OF_CONDUCT.md).

If you aren't a Skywing developer at LLNL, you won't have permission
to push new branches to the repository. First, you should create a
fork. This will create your copy of the Skywing repository and ensure
you can push your changes up to GitHub and create PRs.

* Create your branches off the `repo:main` branch.
* Clearly name your branches, commits, and PRs as this will help us manage queued work in a timely manner.
* Articulate your commit messages in the imperative (e.g., Adds new privacy policy link to README).
* Commit your work in logically organized commits, and group commits together logically in a PR.
* Title each PR clearly and give it an unambiguous description.
* Review existing issues before opening a new one. Your issue might already be under development or discussed by others. Feel free to add to any outstanding issue/bug.
* Be explicit when opening issues and reporting bugs. What behavior are you expecting? What is your justification or use case for the new feature/enhancement? How can the bug be recreated? What are any environment variables to consider?

# Development team
 * Aly Fox <fox33@llnl.gov>
 * Kendall Harter <harter8@llnl.gov>
 * **Colin Ponce <ponce11@llnl.gov>** (corresponding author)
 * Chris Vogl <vogl2@llnl.gov>

# License

Skywing is distributed here under under the GPL v2.0 license, but a
commercial license is also available. Users may choose either license,
depending on their needs.

For the commercial license, please inquire at <softwarelicensing@lists.llnl.gov>.

LLNL-CODE-835832
