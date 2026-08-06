Tutorial 3: Building a Custom Processor
=======================================

In this tutorial, you'll learn how to create a custom processor by implementing the Jacobi iterative method for solving linear systems. This demonstrates how to build your own distributed algorithms in Skywing.

Goal
----

Implement a custom processor that solves a linear system **Ax = b** using the Jacobi iteration method, where each agent owns one or more equations and iteratively updates the solution.

Overview
--------

The Jacobi method is a classic iterative algorithm for solving linear systems. We'll implement the full algorithm from scratch, showing you how to create a custom processor class.

We will use the ``LinearSystemDataLoader`` utility function provided by Skywing to setup the linear system to solve.

The Jacobi Method
-----------------

Given a linear system **Ax = b**, the Jacobi method iteratively updates each variable:

.. math::

   x_i^{(k+1)} = \frac{1}{a_{ii}} \left( b_i - \sum_{j \neq i} a_{ij} x_j^{(k)} \right)

In a distributed setting, each agent:

1. Owns one or more rows (equations) of the matrix
2. Maintains a local copy of the full solution vector **x**
3. Updates its variables based on neighbors' values
4. Broadcasts updated values to neighbors

Example System
~~~~~~~~~~~~~~

We'll solve this 3x3 system:

.. code-block:: text

   4x + y + z = 9
   x + 4y + z = 12
   x + y + 4z = 15

Solution: **x = 1, y = 2, z = 3**

This matrix is diagonally dominant (diagonal elements are larger than off-diagonal sums), which guarantees Jacobi convergence.

Matrix Data Files
-----------------

Skywing uses text files to store matrix problems. In the tutorial folder, there is a directory ``jacobi_data/`` with these files:

**A.txt** - The coefficient matrix, in this example:

.. code-block:: text

   4.0 1.0 1.0
   1.0 4.0 1.0
   1.0 1.0 4.0

**b.txt** - The right-hand side vector:

.. code-block:: text

   9.0
   12.0
   15.0

**row_partition.txt** - Which rows each agent owns (one row per agent):

.. code-block:: text

   0
   1
   2

**col_partition.txt** - Which columns each agent owns (here, each agent owns the full row, i.e. all columns):

.. code-block:: text

   0 1 2
   0 1 2
   0 1 2

Loading Data with LinearSystemDataLoader
-----------------------------------------------

Use Skywing's ``LinearSystemDataLoader`` to load matrix data:

.. code-block:: python

        from skywing.drivers.utils.data_loaders import LinearSystemDataLoader

        data_loader = LinearSystemDataLoader(data_dir, args.agent_id)
        A_local, b_local = data_loader()
        row_partition, col_partition = data_loader.get_partitions()

        # Get this agent's row partition
        my_rows = row_partition[args.agent_id]


This automatically handles:

* Reading the global matrix and vector
* Extracting the local data for this agent
* Managing row/column partitions

Implementing the Custom Processor
----------------------------------

Step 1: Define the Processor Class
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Inherit from ``Processor`` and define the structure:

.. code-block:: python

   from skywing.mid.base_processor import Processor
   import numpy as np

   class SimpleJacobiProcessor(Processor):
       """Solves Ax = b using Jacobi iteration."""

       def __init__(self, data, **kwargs):
           """
           Initialize with matrix data.

           Args:
               data: Tuple of (A_local, b_local)
               **kwargs: Must include 'row_partition' - indices of local rows
           """
           super().__init__(data, **kwargs)

           A_local, b_local = self.data
           self.A_local = A_local
           self.b_local = b_local
           self.row_partition = self.parameters["row_partition"]

           # Full solution vector (all agents need this)
           self.n = A_local.shape[1]
           self.x_global = np.zeros(self.n)

           # Result: (local x values, their indices)
           self.result = (self.x_global[self.row_partition], self.row_partition)

Key initialization steps:

* Store the local matrix and right-hand side
* Get the row partition from parameters
* Initialize the global solution vector
* Set up the result tuple

Step 2: Implement the Core Iteration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``process_update`` method performs one Jacobi iteration:

.. code-block:: python

   def process_update(self, my_tag, recv_data):
       """
        Perform one Jacobi iteration.

        Args:
            my_tag: This agent's unique tag
            recv_data: Dictionary mapping neighbor tags to their data
                      Each neighbor sends (x_values, indices) tuple
        """
        # Update x_global with neighbor values
        for _tag, neighbor_data in recv_data.items():
            self.x_global[neighbor_data.partition] = neighbor_data.values

        # Jacobi update for local rows:
        # x_i = (b_i - sum(A_ij * x_j for j != i)) / A_ii
        # Extract diagonal elements for local rows
        D_inv = np.diag(1.0 / self.A_local[np.arange(len(self.row_partition)), self.row_partition])

        # Compute residual and update
        residual = self.b_local - self.A_local @ self.x_global
        self.x_global[self.row_partition] += D_inv @ residual

        # Update result
        self.result = (self.x_global[self.row_partition], self.row_partition)

The algorithm:

1. **Receive** neighbor x values and update global vector (more on defining the format of the communicated data below)
2. **Compute** the diagonal inverse for local rows
3. **Calculate** residual and update local x values
4. **Store** updated ``self.result``, the processor member variable that is returned by default when calling ``Iteration.query()``

Step 3: Implement additional methods
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**SimpleJacobiData** - Define the format of the communicated data

.. code-block:: python

   class SimpleJacobiData(ProcessorData):
       values: np.ndarray
       partition: list[int]

**prepare_for_publication** - What to send to neighbors:

.. code-block:: python

   def prepare_for_publication(self):
       """Return data to send to neighbors."""
       return SimpleJacobiData(values=self.x_global[self.row_partition], partition=self.row_partition)

Running the Complete Example
-----------------------------

See :doc:`tutorial_max` for additional detail on different methods for running tutorial codes.

Method 1: Using the Shell Script (Easiest)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   ./run_tutorial_3.sh

This launches three agents that collectively solve the system.

Method 2: Manual Launch in Separate Terminals
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Terminal 1:**

.. code-block:: bash

   cd /path/to/Skywing/docs/tutorials
   python tutorial_3_custom_processor.py \
       --port 20000 \
       --nbr-ports 20001,20002 \
       --agent-id 0

**Terminal 2:**

.. code-block:: bash

   cd /path/to/Skywing/docs/tutorials
   python tutorial_3_custom_processor.py \
       --port 20001 \
       --nbr-ports 20000,20002 \
       --agent-id 1

**Terminal 3:**

.. code-block:: bash

   cd /path/to/Skywing/docs/tutorials
   python tutorial_3_custom_processor.py \
       --port 20002 \
       --nbr-ports 20000,20001 \
       --agent-id 2

Expected Output
---------------

Each agent will display convergence to the full solution. Even though agent 0 only
owns/updates the first row (variable x), it maintains and displays the complete
solution vector:

.. code-block:: text

   Starting agent on 127.0.0.1:20000
   Agent ID: 0
   Rows owned: [0]
   Variables: ['x']
   Local matrix shape: (1, 3)
   Neighbors: [20001, 20002]

   Agent 0 - Convergence to Full Solution:
   Agent  Iteration    x            y            z
   --------------------------------------------------------
   0      0            0.000000     0.000000     0.000000
   0      1            2.250000     3.000000     3.750000
   0      2            0.937500     2.015625     2.953125
   0      3            1.003906     1.998047     3.001953
   0      4            0.999512     2.000244     2.999756
   0      5            1.000061     1.999970     3.000031
   0      6            0.999993     2.000004     2.999996
   0      7            1.000001     2.000000     3.000001
   0      8            1.000000     2.000000     3.000000
   0      9            1.000000     2.000000     3.000000

   ==================================================
   Agent 0 - Final Results (Full Solution):
     x: computed=1.000000, expected=1.000000, error=3.45e-09
     y: computed=2.000000, expected=2.000000, error=2.12e-09
     z: computed=3.000000, expected=3.000000, error=1.87e-09
   ==================================================

Notice how all three components converge, even though agent 0 only updates x.
This demonstrates that each agent maintains the full solution vector and receives
updates from neighbors.

Command-Line Arguments
----------------------

The tutorial script accepts:

* ``--port``: Port for this agent (required)
* ``--nbr-ports``: Comma-separated neighbor ports (required)
* ``--agent-id``: Agent ID (0, 1, or 2) - determines which equation(s) this agent owns (required)

The script automatically loads data from ``jacobi_data/`` in the same directory.


Experimenting with the Tutorial
--------------------------------

Try Different Systems
~~~~~~~~~~~~~~~~~~~~~

Modify the matrix files to solve different systems, for example:

.. code-block:: text

   5.0 1.0 1.0
   1.0 5.0 1.0
   1.0 1.0 5.0

Just make sure that is a system where you can expect Jacobi to converge. 

Scale to Larger Systems
~~~~~~~~~~~~~~~~~~~~~~~

For example, create a 6x6 system with 6 agents by expanding the data files. The `LinearSystemDataLoader` will
automatically partition data depending on the partition you provide.
