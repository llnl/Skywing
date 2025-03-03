```
           _|                                  _|
   _|_|_|  _|  _|    _|    _|  _|          _|      _|_|_|      _|_|
 _|_|      _|_|      _|    _|  _|    _|    _|  _|  _|    _|  _|    _|
     _|_|  _|  _|    _|    _|  _|  _|  _|  _|  _|  _|    _|  _|    _|
 _|_|_|    _|    _|    _|_|_|    _|      _|    _|  _|    _|    _|_|_|
                           _|                                      _|
                       _|_|                                    _|_|
```

Skywing Math Interface

# Structure and content of files

## In Skywing math interface - i/o and drivers

* Files:
    * `io/io.hpp` 
        * Functions to convert between Eigen matricies and associative vectors
        * Functions to read a matrix from a file and read a partition from a file
    * `linear_system_driver.hpp`
        * defines the `LinearSystemDriver` class
        * **Constructor parameters:** `configuration, agent_id, A, b, partition, and timeout duration`
        * **Template parameters:** `LinearProcessor, PublishPolicy, StopPolicy, ResiliencePolicy`
        * **To run:** call `linear_system_driver.solve()`

## In skywing mid - processors

 * Files: 
    * `jacobi_processor.hpp`
        * Contains the actual processor to be passed to a Linear System Driver or similar

## In math interface examples

* Files
    * `jacobi.cpp` 
        * The script that recieves arguments and puts everything together
    * `run.py` 
        * Creates the shell scripts needed to make config.cfg and submit job to LC
    * `generate_config_info.py`
        * This is part of the running script, it is run on LC which is why it cannot be part of `run.py`

# Tentative planned structure for future algorithms

## Skywing math interface - drivers, io, and machine setup

* `skywing_math_interface`
    * `drivers` (see below)
        * `linear_system_driver.hpp`
        * `optimization_driver.hpp`
        * `eigenvalue_driver.hpp`
    * `io.hpp` - used for all types of solvers
    * `machine_setup.hpp` - used for all types of solvers

(below) Shared code could be combined into a driver class that takes a type as a template parameter, like `Driver<LinearSystemVariant>' or 'Driver<OptimizationVariant>` depending on how much code ends up being shared.

## Skywing mid - processors

* `linear_system_processors` - these all use a linear system driver
    * `jacobi_processor.hpp`
    * `rejection_weight_jacobi_processor.hpp`
    * `s-ACD_processor.hpp`
* `optimization_processors` - these all can be used in an optimization driver
    * `ADMM_processor.hpp`
    * `gradient_descent`
* `eigenvalue_processors` - these all can be used in en eigenvalue driver

## Skywing math examples - running tools and .cpp files

* .cpp files
    * `jacobi.cpp` 
    * `ADMM.cpp`
    * `eigenvalue_solver.cpp`
* `run.py` - used for all types of solvers, pass solver name such as 'jacobi' to this script
* `generate_config_info.py`- used for all types of solvers

## What is needed to make a new processor

* For a linear system processor
    * `new_linear_system_processor.hpp`
    * `new_linear_system.cpp`
    * Already made: `linear_system_driver.hpp`
* For an optimization algorithm
    * `new_optimization_algorithm_processor.hpp`
    * `new_optimization_algorithm.cpp`
    * Not made yet, but very similar to the linear system driver:  `optimization_driver.hpp`
