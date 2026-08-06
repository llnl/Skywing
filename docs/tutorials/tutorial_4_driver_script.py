"""
Tutorial 4: Custom Driver-Compatible Script

This script demonstrates how to create your own script that works with the Skywing driver.
The driver will automatically launch multiple instances of this script with appropriate
arguments for distributed execution.

Usage with driver:
    python -m skywing.drivers.driver docs/tutorials/tutorial_4_driver_script.py \\
        --num_agents 5 \\
        --comm_topology full \\
        --kwargs num_calls=10 sleep_time=0.5 \\
        --print

The driver automatically provides:
    - Agent ID and network configuration
    - Neighbor connections
    - Optional asynchrony settings
"""

import time

from skywing.drivers.utils.agent_utils import create_skywing_agent
from skywing.drivers.utils.argparse_utils import (
    argparse_list_to_kwargs,
    parse_driver_command_line,
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
    agent = create_skywing_agent(address, port, nbr_addresses, nbr_ports, test_async)

    # Create a processor with local data
    # In this example, each agent starts with its ID as the initial value
    processor = MaxProcessor(agent_id * 10.0)
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
