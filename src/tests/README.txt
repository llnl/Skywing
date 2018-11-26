This directory contains Skynet tests, which make use of the Catch2 library.

In order to use these tests, you must perform the following steps to obtain
the Catch2 library:

1) From the Skynet root directory, run 'git submodule init'.
2) Run 'git submodule update'

At this point, you should be able to compile all tests with 'make all' or 
tests for a particular MODULE with 'make MODULE'.
