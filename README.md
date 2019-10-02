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
 * Amy Musselman <musselman5@llnl.gov>
 * Colin Ponce <ponce11@llnl.gov>
 * Chris Vogl <vogl2@llnl.gov>

## Dependencies
 * meson (https://mesonbuild.com/)
 * ninja (https://ninja-build.org/)
 * ns3 (see subprojects/ns3/build_ns3)
 * Managed automatically:
   * Catch2
   * zmqcpp

## Build instructions
 * Get dependencies
   * git submodule init
   * git submodule update
 * Create build files
   * meson <BUILD_DIRECTORY>
 * Build Skynet
   * cd <BUILD_DIRECTORY>
   * ninja
 * Run tests
   * ninja test
