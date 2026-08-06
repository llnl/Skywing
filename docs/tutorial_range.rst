Tutorial 2: Running Multiple Iterations
========================================

In this tutorial, you'll learn how to run multiple iterations simultaneously and update one iteration based on results from another. We'll compute the range (max - min) of values across a distributed network.

Goal
----

We will have the collective compute both the maximum and range of their values using two concurrent iterations that communicate with each other.


Overview
--------

This tutorial demonstrates a more advanced pattern: running multiple iterations on the same agent. We'll compute the range (max - min) across agents with these values:

* Agent 1: value = 42
* Agent 2: value = 73
* Agent 3: value = 15

The expected results:

* Maximum = 73
* Minimum = 15
* Range = 73 - 15 = 58

The Challenge
-------------

Computing the range requires finding both the maximum and minimum. We could run separate max and min iterations, but to illustrate using
the result of one iteration as input to another, we will take a different approach:

1. Find the maximum value across all agents
2. Each agent computes its distance from the maximum: ``distance = max - local_value``
3. Find the maximum distance across all agents 

This maximum distance will be the range of the data.

For example, if the max is 73 and the min is 15, then:

* The agent with max (73) has distance: 73 - 73 = 0
* The agent with min (15) has distance: 73 - 15 = 58

The maximum distance (58) equals the range!

Multiple Iterations on One Agent
---------------------------------

Skywing allows you to run multiple iterations on the same agent simultaneously. Each iteration operates independently with its own processor:

.. code-block:: python

    agent = Agent(address, args.port)

    # Configure neighbors
    neighbors = [(address, p) for p in neighbor_ports]
    agent.configure_neighbors(neighbors)

    # Create max processor with local value
    max_processor = MaxProcessor(args.local_value)
    max_iteration = Iteration(max_processor, agent)
    max_iteration.launch()

    # Create distance processor (starts at 0, will be updated)
    distance_processor = MaxProcessor(0.0)
    distance_iteration = Iteration(distance_processor, agent)
    distance_iteration.launch()

Key points:

* Both iterations share the same ``agent`` and its network connections
* Each iteration has its own processor
* Skywing handles message routing automatically using unique tags
* Iterations run concurrently and independently

Updating Data Dynamically
--------------------------

The second iteration needs to be updated as we learn the maximum. We use ``update_data()`` to change an iteration's local data:

.. code-block:: python

   # Query the current maximum
   max_result = max_iteration.query()

   # Calculate distance from maximum
   distance_from_max = max_result - local_value

   # Update the distance iteration with new data
   distance_iteration.update_data(distance_from_max)

   # Query the maximum distance
   max_distance = distance_iteration.query()

   # The range is the maximum distance
   range_value = max_distance

The ``update_data()`` method tells the processor to use a new value in subsequent iterations of the algorithm.

Handling Changing Local Values
+++++++++++++++++++++++++++++++

A key feature of Skywing is its ability to adapt to changing data. In this tutorial, each agent's local value is periodically updated to demonstrate how the system reconverges to the correct answer.

After 5 queries, each agent updates its local value to a new random number:

.. code-block:: python

  for query_num in range(10):
        time.sleep(0.5)

        # Update local value every 5 queries to demonstrate adaptability
        if query_num > 0 and query_num % 5 == 0:
            current_local_value = random.uniform(10, 80)
            print(f"  [Agent {args.port} updated local value to {current_local_value:.2f}]")
            max_iteration.update_data(current_local_value)

      # [above code here]

This demonstrates several important properties:

* **Resilience**: The algorithm adapts to changing data without manual intervention
* **Continuous Operation**: Both iterations continue to converge as the streaming data changes

When you run the tutorial, you'll see output like:

.. code-block:: text

   Port     Query    Local Val    Max         Distance    Range
   --------------------------------------------------------------------
   20000    0        42.00        42.00       0.00        0.00        0.00
   20000    1        42.00        73.00       31.00       31.00       31.00
   20000    2        42.00        73.00       31.00       58.00       58.00
     [Agent 20000 updated local value to 65.43]
   20000    3        65.43        73.00       7.57        58.00       58.00
   20000    4        65.43        73.00       7.57        58.00       58.00
   20000    5        65.43        73.00       7.57        58.00       58.00
     [Agent 20000 updated local value to 28.91]
   20000    6        28.91        73.00       44.09       58.00       58.00
   ...

Notice how when an agent updates its value (e.g., from 42.00 to 65.43), the Distance column changes immediately, and the algorithm continues to track the correct maximum and range across all agents.

Running the Tutorial Code
-------------------------

See :doc:`tutorial_max` for additional detail on different methods for running tutorial codes.

Method 1: Using the Shell Script
+++++++++++++++++++++++++++++++++

.. code-block:: bash

   cd /path/to/Skywing
   ./docs/tutorials/run_tutorial_2.sh

This launches three agents that each run 10 queries, printing their progress as they compute the range.

Method 2: Manual Launch
+++++++++++++++++++++++

**Terminal 1:**

.. code-block:: bash

   python tutorial_2_range.py \
       --port 20000 \
       --nbr-ports 20001,20002 \
       --local-value 42

**Terminal 2:**

.. code-block:: bash

   python tutorial_2_range.py \
       --port 20001 \
       --nbr-ports 20000,20002 \
       --local-value 73

**Terminal 3:**

.. code-block:: bash

   python tutorial_2_range.py \
       --port 20002 \
       --nbr-ports 20000,20001 \
       --local-value 15


Expected Output
---------------

Each agent will display output showing convergence, something like this:

.. code-block:: text

   Starting agent on 127.0.0.1:20000
   Local value: 42.0
   Neighbors: [20001, 20002]

   Port     Query    Local Val    Max          Distance     Range
   --------------------------------------------------------------------
   20000    0        42.00        42.00        0.00         0.00
   20000    1        42.00        73.00        31.00        31.00
   20000    2        42.00        73.00        31.00        58.00
   20000    3        42.00        73.00        31.00        58.00
   20000    4        42.00        73.00        31.00        58.00
   ...


Notice how the values converge:

* **Query 0**: Only knows its own value (42), so max=42, distance=0
* **Query 1**: Learned the true max (73), calculates distance=31
* **Query 2**: Learns the maximum distance (58), which is the range!

Command-Line Arguments
-----------------------

The tutorial script accepts these arguments:

* ``--port``: Port for this agent (required)
* ``--nbr-ports``: Comma-separated neighbor ports (required)
* ``--local-value``: Initial value for this agent (required)

The script runs 10 queries with a 0.5-second delay between queries, and updates local values after 5 queries to demonstrate dynamic adaptation.


Experimenting with the Tutorial
--------------------------------

**Try Different Value Distributions**

.. code-block:: bash

   # Terminal 1: Large range
   python docs/tutorials/tutorial_2_range.py --port 20000 --nbr-ports 20001,20002 --local-value 10

   # Terminal 2
   python docs/tutorials/tutorial_2_range.py --port 20001 --nbr-ports 20000,20002 --local-value 50

   # Terminal 3
   python docs/tutorials/tutorial_2_range.py --port 20002 --nbr-ports 20000,20001 --local-value 100

Expected range: 100 - 10 = 90

**Add More Agents**

Scale to 4 agents:

.. code-block:: bash

   # Agent 1
   python docs/tutorials/tutorial_2_range.py --port 20000 --nbr-ports 20001,20002,20003 --local-value 25

   # Agent 2
   python docs/tutorials/tutorial_2_range.py --port 20001 --nbr-ports 20000,20002,20003 --local-value 88

   # Agent 3
   python docs/tutorials/tutorial_2_range.py --port 20002 --nbr-ports 20000,20001,20003 --local-value 12

   # Agent 4
   python docs/tutorials/tutorial_2_range.py --port 20003 --nbr-ports 20000,20001,20002 --local-value 67

Expected range: 88 - 12 = 76

**Test Convergence Speed**

Compare fully connected vs. ring topology:

.. code-block:: bash

   # Ring topology - slower convergence
   python docs/tutorials/tutorial_2_range.py --port 20000 --nbr-ports 20001,20003 --local-value 10
   python docs/tutorials/tutorial_2_range.py --port 20001 --nbr-ports 20000,20002 --local-value 50
   python docs/tutorials/tutorial_2_range.py --port 20002 --nbr-ports 20001,20003 --local-value 100
   python docs/tutorials/tutorial_2_range.py --port 20003 --nbr-ports 20002,20000 --local-value 150

You'll notice convergence might take more queries with sparse topologies.

Alternative: Using MinProcessor
--------------------------------

Instead of the distance trick, you could run separate Max and Min iterations:

.. code-block:: python

   from skywing.mid import MaxProcessor, MinProcessor

   # First iteration: find maximum
   max_processor = MaxProcessor(args.local_value)
   max_iteration = Iteration(max_processor, agent)
   max_iteration.launch()

   # Second iteration: find minimum
   min_processor = MinProcessor(args.local_value)
   min_iteration = Iteration(min_processor, agent)
   min_iteration.launch()

   # Query both
   max_result = max_iteration.query()
   min_result = min_iteration.query()
   range_value = max_result - min_result

Both approaches work! The distance method demonstrates how the output of one iteration can be the input of another. 

Key Takeaways
-------------

* Multiple iterations can run concurrently on the same agent
* Each iteration has its own processor and state
* Use ``update_data()`` to dynamically change an iteration's data
* One iteration can depend on results from another
* Convergence requires some time as information propagates through the network

Next Tutorial
-------------

Continue to :doc:`tutorial_custom` (Tutorial 3) to learn about building custom processors for specialized distributed algorithms.
