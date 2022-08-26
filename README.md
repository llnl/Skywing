```
           _|                                        _|
   _|_|_|  _|  _|    _|    _|  _|_|_|      _|_|    _|_|_|_|
 _|_|      _|_|      _|    _|  _|    _|  _|_|_|_|    _|
     _|_|  _|  _|    _|    _|  _|    _|  _|          _|
 _|_|_|    _|    _|    _|_|_|  _|    _|    _|_|_|      _|_|
                           _|
                       _|_|
```
A high-reliability, real-time, decentralized platform for collaborative autonomy.

## Development team
 * Aly Fox <fox33@llnl.gov>
 * Kendall Harter <harter8@llnl.gov>
 * **Colin Ponce <ponce11@llnl.gov>** (corresponding author)
 * Chris Vogl <vogl2@llnl.gov>

## Dependencies Not Automatically Managed
 * compiler that supports c++17 library
   * tested: GCC/g++ (8.3.0) and LLVM/clang (6.0.0, 10.0.0, 11.0.0)
 * meson (https://mesonbuild.com/)
   * requires Python >= 3.8.0
 * CMake (https://cmake.org/)
 * ninja (https://ninja-build.org/)
 * Cap'n Proto (https://capnproto.org/)
   * requires version 0.8.0 or newer

## Dependencies Managed as Git Submodules
 * Catch2
 * spdlog
 * Guidelines Support Library (GSL)

## Build instructions
 * Build non-managed dependencies separately
 * Get dependencies
   * git submodule update --init
 * Create build files
   * `mkdir build`
   * `meson build`
 * Build Skywing
   * `cd build`
   * `ninja`
 * Run tests
   * `meson test --print-errorlogs -t 5.0`

## Enabling Tests and Examples

### Before creating the build directory
 * `meson build -Dbuild_tests=true -Dbuild_examples=true`

### After creating the build directory
 * Move to the build directory
 * `meson configure -Dbuild_tests=true -Dbuild_examples=true`

## Guidance for building on LC

### Building capnp
 * Cap'n Proto must be manually built first. Follow the instructions at https://capnproto.org/install.html#installation-unix, except you must build to a local directory. To do this, on the configure step, use
   * `./configure --prefix=capnp_build_dir`
   * This will create subdirectories `capnp_build_dir/bin`, `capnp_build_dir/include`, and `capnp_build_dir/lib`

### Building Skywing
 * Load meson (python), ninja, and switch to more recent version of gcc
   * 'ml python/3.8.2`
   * `ml ninja`
   * `ml gcc/8.3.1`
 * Add capnp pkgconfig directory to PKG_CONFIG_PATH
   * `export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:capnp_build_dir/lib/pkgconfig`
 * Follow build instructions as normal
 * To build the LC Hello World example, also include `-Dbuild_lc_examples=true` in the meson options.
 * To run the LC example, go to `(skywing_root)/build/examples/lc_hello_world/` and execute `source run.sh (bank_name)`. Note that you must have an active bank to run this test.

### Running Skywing
 * Add capnp shared library to LD_LIBRARY_PATH
   * `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:capnp_build_dir/lib`
 * Run as normal for running on login node. Note: can't run long jobs on login nodes!
 * Skywing configurations that involve many connections between agents can run into a file descriptor limit.  The soft limit can be increased by executing `ulimit -n <N>` where `<N>` must not exceed the hard limit (which is determined by executing `ulimit -Hn`) 

Note that Skywing configurations that involve many connections between agents can run into a file descriptor limit.
The soft limit can be increased by executing `ulimit -n <N>` where `<N>` must not exceed the hard limit (which can be determined by executing `ulimit -Hn`)


---

The skywing_upper name is a temporary placeholder name for higher parts of the library that build on the lower parts, which are in skywing_core.
