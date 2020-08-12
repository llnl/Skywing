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
 * Corey McNeish <mcneish1@llnl.gov>
 * Colin Ponce <ponce11@llnl.gov>
 * Chris Vogl <vogl2@llnl.gov>
 * Kendall Harter <harter8@llnl.gov>

## Dependencies Not Automatically Managed
 * compiler that supports c++17 library
   * tested: GCC/g++ (8.3.0) and LLVM/clang (6.0.0, 10.0.0, 11.0.0)
 * meson (https://mesonbuild.com/)
   * requires Python >= 3.5.2
 * CMake (https://cmake.org/)
 * ninja (https://ninja-build.org/)
 * Cap'n Proto (https://capnproto.org/)
   * requires version 0.7.0 or newer

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
 * Build Skynet
   * `cd build`
   * `ninja`
 * Run tests
   * `ninja test`

## Enabling Tests and Examples

### Before creating the build directory
 * `meson build -Dbuild_tests=true -Dbuild_examples=true`

### After creating the build directory
 * Move to the build directory
 * `meson configure -Dbuild_tests=true -Dbuild_examples=true`

---

The skynet_upper name is a temporary placeholder name for higher parts of the library that build on the lower parts, which are in skynet_core.
