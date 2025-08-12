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
        * **Template parameters:** `LinearProcessor, PublishPolicy, IterationPolicy, ResiliencePolicy`
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

# Linear system processors

The processors defined in `skywing_mid/linear_system_processors` are meant to be used in conjunction with
the the linear system driver defined in the math interface: `skywing_math_interface/linear_system_driver.py`.
These processors must implement the following standard in addition to the usual processor functions.

The following types must be defined:
```
OpenVector
ClosedVector
ClosedMatrix
ValueType
IndexType
ScalarType
```

The constructor must have the form:
```
Processor(ClosedMatrix A, ClosedVector b) {}
```

Additional parameters or objects needed by the processor may be passed by the routine:
```
void set_parameters(...) {}
```
which may take arbitrary arguments.
