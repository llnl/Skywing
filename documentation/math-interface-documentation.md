@defgroup math-interface

```
           _|                                  _|
   _|_|_|  _|  _|    _|    _|  _|          _|      _|_|_|      _|_|
 _|_|      _|_|      _|    _|  _|    _|    _|  _|  _|    _|  _|    _|
     _|_|  _|  _|    _|    _|  _|  _|  _|  _|  _|  _|    _|  _|    _|
 _|_|_|    _|    _|    _|_|_|    _|      _|    _|  _|    _|    _|_|_|
                           _|                                      _|
                       _|_|                                    _|_|
```

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
