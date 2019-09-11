This directory contains Skynet tests, which make use of the Catch2 library.

In order to use these tests, you must perform the following steps to obtain
the Catch2 library:

1) From the Skynet root directory, run 'git submodule init'.
2) Run 'git submodule update'

With the above steps complete, you can build and the run either:

1) tests for all modules
   a) make all
   b) ./run_catch_tests
2) tests only for a specific MODULE
   a) make MODULE_tests
   b) ./MODULE/run_catch_MODULE_tests
