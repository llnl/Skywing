Tutorial 4: Using the Driver for Research
==========================================

This tutorial is designed for researchers and algorithm developers who want to quickly experiment
with different distributed algorithms and configurations without writing custom agent code.

Goal
----

Learn to use Skywing's driver and ``simple_iteration.py`` to rapidly test different processors,
network topologies, and configurations for research and algorithm development.

Overview
--------

The Skywing driver (``skywing.drivers.driver``) is a tool that:

* Automatically launches multiple agent subprocesses as a Skywing collective on a local machine
* Configures network topologies (full, ring, line, or custom from file)
* Passes standardized arguments to **any driver-compatible script**
* Handles output collection
* Supports testing with artificial asynchrony


The Driver Command
------------------

Basic Syntax
~~~~~~~~~~~~

You can launch a Skywing collective on a local machine to run any python script via:

.. code-block:: bash

   python -m skywing.drivers.driver <script> [options]

or use the ``skywing_drive`` alias:

.. code-block:: bash

   skywing_drive <script> [options]

The driver launches multiple agent processes that each run the specified script.

**The script can be any Python script that follows the driver interface.**

* Use the provided ``simple_iteration.py`` - a generic script that works with many processors
* Write completely custom driver-compatible scripts for specialized workflows (see `Creating Custom Driver-Compatible Scripts`_ below)

The driver handles all the agent coordination; your script just implements the per-agent logic.


Understanding Arguments: Driver vs. Script
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The driver accepts two types of arguments:

**1. Driver Configuration (Direct Arguments)**

These configure the driver itself and control how agents are launched:

* ``--num_agents N``: Number of agents in the collective (default: 3)
* ``--starting_port PORT``: Starting port number (default: 20000)
* ``--comm_topology TOPO``: Network topology: ``full``, ``ring``, or ``line`` (default: full)
* ``--config FILE``: JSON file with custom collective configuration (default: None, optional if port and topology are provided)
* ``--print``: Print agent outputs to screen (default: False, disabled)
* ``--unbuffered or -u``: Stream agent outputs in real-time (default: False, buffered output)
* ``--test_async PROB ITER_PROB DELAY``: Simulate asynchrony for testing (default: None, no artificial asynchrony)


**2. Script Arguments (via kwargs)**

These are passed through to your script (e.g., ``simple_iteration.py``):

.. code-block:: bash

   --kwargs processor=Max num_calls=10 sleep_time=1.0

The ``--kwargs`` parameters are **not used by the driver**—they are forwarded to your script's ``args.kwargs`` dictionary. What kwargs are valid depends entirely on the script you're running.

**What the Driver Provides to Your Script**

When the driver launches your script, it **automatically provides** these arguments:

* ``--agent_id``: Unique agent ID (integer starting from 0)
* ``--address``: Agent's IP address (e.g., "127.0.0.1")
* ``--port``: Agent's port (e.g., 20000)
* ``--nbr_addresses``: List of neighbor IP addresses
* ``--nbr_ports``: List of neighbor ports
* ``--test_async``: Optional asynchrony test parameters (if specified)
* ``--kwargs``: Your custom key=value pairs as a dictionary

Your script is responsible for using these arguments, and Skywing provides some helper funcitons
for parsing these arguments, creating agents, etc. in ``skywing.drivers.utils``.
You can see an example of how each is used in ``simple_iteration.py``.

Understanding simple_iteration.py
----------------------------------

What is simple_iteration.py?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``simple_iteration.py`` is a **driver-compatible reference implementation** that demonstrates 
a **generic test harness** for trying any built-in processor, and an **example of the driver interface** showing what your custom scripts need to implement


**You can use it directly** for standard experiments, or use it as a reference for your own driver-compatible script.

Available Processors in simple_iteration.py
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pass via ``--kwargs processor=<name>``:

* ``Max`` - Maximum consensus
* ``Min`` - Minimum consensus
* ``SimpleSum`` - Sum aggregation
* ``PushSum`` - Distributed averaging
* ``SGD`` - Stochastic gradient descent
* ``ADMM`` - Alternating direction method of multipliers
* ``COLA`` - Collaborative optimization
* ``Jacobi`` - Iterative linear solver
* ``SONATA`` - Advanced optimization

**To see available processors or add your own processor**, see the ``PROCESSOR_OPTIONS`` dictionary in ``simple_iteration.py``.

Common Usage Scenarios
----------------------

Basic Example
~~~~~~~~~~~~~

Find the maximum value across 5 agents:

.. code-block:: bash

   python -m skywing.drivers.driver ./python/examples/simple_iteration.py --num_agents 5 --kwargs processor=Max --print

Testing Network Topology Effects
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Network topology significantly affects convergence speed and algorithm behavior. Use ``--comm_topology`` to compare:

.. code-block:: bash

   # Full connectivity (fastest convergence, highest communication overhead)
   python -m skywing.drivers.driver python/examples/simple_iteration.py --num_agents 10 --comm_topology full --kwargs processor=Max --print

   # Ring topology (slower convergence, lower communication)
   python -m skywing.drivers.driver python/examples/simple_iteration.py --num_agents 10 --comm_topology ring --kwargs processor=Max --print

This demonstrates how sparse networks slow information propagation across the collective.

Testing Algorithm Robustness with Asynchrony
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Real distributed systems have stragglers and network delays. Test robustness using ``--test_async``:

.. code-block:: bash

   python -m skywing.drivers.driver python/examples/simple_iteration.py --test_async 0.4 0.3 2.0 --kwargs processor=Max --print

This simulates 40% of agents experiencing delays in 30% of iterations with 2-second delays. Algorithms should still converge despite asynchrony.

Saving Results for Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For systematic experiments, save per-agent results to files instead of printing. First, create the output directory:

.. code-block:: bash

   mkdir -p results
   python -m skywing.drivers.driver python/examples/simple_iteration.py --kwargs processor=Max output_dir=results num_calls=50

This creates ``results/output_0.npy``, ``results/output_1.npy``, etc., with numpy representations of the iteration results queried at each "call" in the main execution loop in ``simple_iteration.py``.

**Note:** The ``simple_iteration.py`` script does not automatically create the output directory, so you must create it before running the command.

Discovering Available Parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``simple_iteration.py`` accepts many parameters for controlling execution and configuring processors. Rather than maintaining exhaustive tables here, use these resources to find current parameters:

**For script-level parameters** (like ``num_calls``, ``sleep_time``, ``output_dir``):

.. code-block:: bash

   python python/examples/simple_iteration.py --help

**For processor-specific parameters**

* See the **Processors API documentation**: :doc:`processors`
* Each processor's documentation lists its parameters, defaults, and descriptions

**Common pattern**: Most processors accept algorithm-specific tuning parameters as kwargs:

.. code-block:: bash

   # SGD example with learning rate and regularization
   --kwargs processor=SGD gamma=0.05 lam=0.1 partitioning=row

   # ADMM example with penalty parameter
   --kwargs processor=ADMM rho=1.5 alpha=1.0

Example Usage with Different Processors
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Basic consensus (Max processor):**

.. code-block:: bash

   python -m skywing.drivers.driver python/examples/simple_iteration.py --num_agents 5 --kwargs processor=Max --print

**SGD with custom parameters (using auto-generated data):**

.. code-block:: bash

   python -m skywing.drivers.driver python/examples/simple_iteration.py --num_agents 3 --kwargs processor=SGD data_type=matrix partitioning=row gamma=0.05 lam=0.1 --print

**SGD with existing test data:**

.. code-block:: bash

   python -m skywing.drivers.driver python/examples/simple_iteration.py --num_agents 3 --kwargs processor=SGD data_type=matrix data_file=python/tests/test_data/random_matrix_row_partitioned_3 partitioning=row gamma=0.05 lam=0.1 --print

**Tips:**

- If you omit ``data_file``, ``simple_iteration.py`` generates random data automatically
- For custom data, provide a directory with ``A.txt``, ``b.txt``, and partition files
- Example data is available in ``python/tests/test_data/`` for testing

Using JSON Configuration Files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For custom network topologies, you can use a JSON configuration file. Several example configs are provided in ``python/tests/test_configs/``:

**3-agent full connectivity** (``3_full.json``):

.. code-block:: json

   [
       {"id": 0, "address": "127.0.0.1", "port": 20000, "nbrs": [0, 1, 2]},
       {"id": 1, "address": "127.0.0.1", "port": 20001, "nbrs": [0, 1, 2]},
       {"id": 2, "address": "127.0.0.1", "port": 20002, "nbrs": [0, 1, 2]}
   ]

**3-agent line topology** (``3_line.json``):

.. code-block:: json

   [
       {"id": 0, "address": "127.0.0.1", "port": 20000, "nbrs": [0, 1]},
       {"id": 1, "address": "127.0.0.1", "port": 20001, "nbrs": [0, 1, 2]},
       {"id": 2, "address": "127.0.0.1", "port": 20002, "nbrs": [1, 2]}
   ]

**Using a config file:**

.. code-block:: bash

   python -m skywing.drivers.driver python/examples/simple_iteration.py --config python/tests/test_configs/3_line.json --kwargs processor=Max --print

You can also create your own JSON config file following the same format. Each agent entry specifies its ID, IP address, port, and a list of neighbor IDs it should connect to.


Creating Custom Driver-Compatible Scripts
------------------------------------------

Requirements for Driver-Compatible Scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Any Python script can be driver-compatible** by following these requirements:

1. **Accept driver arguments** using ``parse_driver_command_line()``
2. **Create an agent** using the provided address, port, and neighbors
3. **Implement your algorithm** using any processor or custom logic
4. **Exit cleanly** when done

The driver will automatically provide the standardized arguments listed in the "Understanding Arguments" section above.

Creating a Custom Script
~~~~~~~~~~~~~~~~~~~~~~~~~

You can either adapt ``simple_iteration.py`` or write from scratch. Here's the complete example from ``tutorial_4_driver_script.py``:

.. code-block:: python

   import time
   from skywing.drivers.utils.agent_utils import create_skywing_agent
   from skywing.drivers.utils.argparse_utils import (
       parse_driver_command_line,
       argparse_list_to_kwargs,
   )
   from skywing.mid import Iteration, MaxProcessor

   def main():
       """Main function for the custom driver script."""
       # Parse arguments provided by the driver
       args = parse_driver_command_line()

       # Access driver-provided arguments
       agent_id = args.agent_id
       address = args.address
       port = args.port
       nbr_addresses = args.nbr_addresses
       nbr_ports = args.nbr_ports
       test_async = args.test_async

       # Convert kwargs list to dictionary
       kwargs = argparse_list_to_kwargs(args.kwargs)

       # Access custom kwargs
       num_calls = kwargs.get("num_calls", 10)
       sleep_time = kwargs.get("sleep_time", 1.0)
       output_dir = kwargs.get("output_dir", None)

       print(f"Agent {agent_id} starting on {address}:{port}")
       print(f"Connected to {len(nbr_ports)} neighbors")

       # Create Skywing agent with the provided configuration
       agent = create_skywing_agent(
           address, port, nbr_addresses, nbr_ports, test_async
       )

       # Create a processor with local data
       # In this example, each agent starts with its ID as the initial value
       processor = MaxProcessor(agent_id * 10)
       print(f"Agent {agent_id} initial value: {agent_id * 10}")

       # Create and launch the iteration
       iteration = Iteration(processor, agent)
       iteration.launch()

       # Query results periodically
       results = []
       for i in range(int(num_calls)):
           time.sleep(float(sleep_time))
           result = iteration.query()
           results.append((i, result))
           print(f"Agent {agent_id} at query {i}: {result}")

       # Save results to file if output directory is specified
       if output_dir:
           import os

           os.makedirs(output_dir, exist_ok=True)
           output_file = os.path.join(output_dir, f"output_{agent_id}.txt")
           with open(output_file, "w") as f:
               for query_num, value in results:
                   f.write(f"{query_num},{value}\n")
           print(f"Agent {agent_id} results saved to {output_file}")

       # Final result
       final_result = iteration.query()
       print(f"Agent {agent_id} final result: {final_result}")

   if __name__ == "__main__":
       main()

Run this script with the driver:

.. code-block:: bash

   python -m skywing.drivers.driver docs/tutorials/tutorial_4_driver_script.py --num_agents 5 --kwargs num_calls=10 sleep_time=0.5 --print

Making Your Script Driver-Compatible
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To make any script work with the driver:

1. **Import the utilities:**

   .. code-block:: python

      from skywing.drivers.utils.agent_utils import create_skywing_agent
      from skywing.drivers.utils.argparse_utils import (
          parse_driver_command_line,
          argparse_list_to_kwargs,
      )

2. **Parse arguments the driver provides:**

   .. code-block:: python

      args = parse_driver_command_line()
      agent_id = args.agent_id
      address = args.address
      port = args.port
      nbr_addresses = args.nbr_addresses
      nbr_ports = args.nbr_ports
      test_async = args.test_async

3. **Convert kwargs list to dictionary:**

   .. code-block:: python

      kwargs = argparse_list_to_kwargs(args.kwargs)

4. **Access your custom kwargs:**

   .. code-block:: python

      num_calls = kwargs.get("num_calls", 10)
      sleep_time = kwargs.get("sleep_time", 1.0)
      output_dir = kwargs.get("output_dir", None)

5. **Create the agent with the driver's configuration:**

   .. code-block:: python

      agent = create_skywing_agent(
          address, port, nbr_addresses, nbr_ports, test_async
      )

6. **Implement your algorithm** (create processor, iteration, query results, etc.)
