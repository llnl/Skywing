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
 * Adam Harter <harter8@llnl.gov>
 * Aly Fox <fox33@llnl.gov>
 * Corey McNeish <mcneish1@llnl.gov>
 * Colin Ponce <ponce11@llnl.gov>
 * Chris Vogl <vogl2@llnl.gov>

## Dependencies Not Automatically Managed
 * meson (https://mesonbuild.com/)
 * CMake (https://cmake.org/)
 * ninja (https://ninja-build.org/)
 * Cap'n Proto (https://capnproto.org/)

## Dependencies Managed as Git Submodules
 * Catch2
 * Spd Log
 * Guidelines Support Library (GSL)

## Build instructions
 * Build non-managed dependencies separately
 * Get dependencies
   * git submodule update --init
 * Create build files
   * mkdir build
   * meson build
 * Build Skynet
   * cd build
   * ninja
 * Run tests
   * ninja test

---

The skynet_upper name is a temporary placeholder name for higher parts of the library that build on the lower parts, which are in skynet_core.
