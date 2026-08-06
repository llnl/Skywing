Tutorials
=========

This section provides step-by-step tutorials to help you get started with Skywing.
Each tutorial builds on the previous ones, introducing new concepts and techniques.

Tutorial Overview
-----------------

.. toctree::
   :maxdepth: 1

   tutorial_max
   tutorial_range
   tutorial_custom
   tutorial_driver

Tutorial 1: Your First Distributed Computation
-----------------------------------------------

Learn the basics by creating agents that collaboratively compute the maximum value across a network.
This tutorial covers the fundamental concepts of agents, processors, and iterations.

:doc:`tutorial_max`

Tutorial 2: Running Multiple Iterations
----------------------------------------

Learn how to run multiple iterations simultaneously on the same agent and update one
iteration based on results from another. This tutorial demonstrates computing the range
(max - min) using concurrent iterations.

:doc:`tutorial_range`

Tutorial 3: Building Custom Processors
---------------------------------------

Create your own custom processors to implement specialized distributed algorithms.
Learn the processor interface and best practices for building robust algorithms.

:doc:`tutorial_custom`

Tutorial 4: Using the Built-in Driver for Research
---------------------------------------------------

Learn to use Skywing's driver and ``simple_iteration.py`` for rapid experimentation.
This tutorial is designed for researchers and algorithm developers who want to quickly
test different processors, topologies, and configurations without writing custom agent code.

:doc:`tutorial_driver`
