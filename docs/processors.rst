Included Algorithms
===================

Processors are the core algorithmic components in Skywing that implement distributed algorithms.
Each processor encapsulates the logic for a specific algorithm and can be used with the Iteration
framework to run distributed computations.

Overview
--------

All processors inherit from the ``BaseProcessor`` class and implement the processor interface.
Processors handle:

* Local state management
* Processing incoming data from neighbors
* Preparing outgoing data to send to neighbors
* Convergence detection and stopping conditions

Algorithm Categories
--------------------

:doc:`consensus_processors`
    Processors for computing aggregate values (max, min, sum, average) across the network using consensus algorithms.

:doc:`optimization_processors`
    Distributed optimization algorithms for machine learning and optimization problems, including SGD, ADMM, COLA, and SONATA.

:doc:`linear_solver_processors`
    Iterative methods for solving systems of linear equations in a distributed manner.

.. toctree::
   :maxdepth: 2

   consensus_processors
   optimization_processors
   linear_solver_processors
