"""
Tutorial 2: Running Multiple Iterations
Computing range (max - min) using two concurrent iterations.

This script demonstrates how to run multiple iterations simultaneously and
update one iteration based on results from another.

Usage:
    python docs/tutorials/tutorial_2_range.py --port 20000 --nbr-ports 20001,20002 --local-value 42
"""

import argparse
import random
import time

from skywing.core import Agent
from skywing.mid import Iteration, MaxProcessor


def main():
    parser = argparse.ArgumentParser(
        description="Tutorial 2: Multiple iterations - computing range across distributed agents"
    )
    parser.add_argument(
        "--port", type=int, required=True, help="Port for this agent to listen on"
    )
    parser.add_argument(
        "--nbr-ports",
        type=str,
        required=True,
        help="Comma-separated list of neighbor ports (e.g., '20001,20002')",
    )
    parser.add_argument(
        "--local-value",
        type=float,
        required=True,
        help="Initial local value for this agent",
    )
    parser.add_argument(
        "--no-print-header",
        dest="print_header",
        action="store_false",
        help="Don't print header info",
    )

    args = parser.parse_args()

    # Parse neighbor ports
    neighbor_ports = [int(p.strip()) for p in args.nbr_ports.split(",")]

    # Fixed address for simplicity
    address = "127.0.0.1"

    # Create agent
    if args.print_header:
        print(f"Starting agent on {address}:{args.port}")
        print(f"Local value: {args.local_value}")
        print(f"Neighbors: {neighbor_ports}")

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

    # Wait for initial convergence
    time.sleep(0.5)

    # Track current local value (starts with initial value)
    current_local_value = args.local_value

    # Query results multiple times
    if args.print_header:
        print(
            f"\n{'Port':<8} {'Query':<8} {'Local Val':<12} {'Max':<12} {'Distance':<12} {'Range':<12}"
        )
        print("-" * 68)

    distance_from_max = 0
    range_value = 0

    for query_num in range(10):
        time.sleep(1)

        # Update local value every 5 queries to demonstrate adaptability
        if query_num > 0 and query_num % 5 == 0:
            current_local_value = random.uniform(10, 80)
            print(
                f"  [Agent {args.port} updated local value to {current_local_value:.2f}]"
            )
            max_iteration.update_data(current_local_value)

        # Query the current maximum
        max_result = max_iteration.query()

        # Calculate this agent's distance from the max
        if max_result is not None:
            distance_from_max = max_result - current_local_value

        # Update the distance iteration with the new distance
        distance_iteration.update_data(distance_from_max)

        # Query the maximum distance (current estimate of the range)
        range_value = distance_iteration.query()

        print(
            f"{args.port:<8} {query_num:<8} {current_local_value:<12.2f} {max_result:<12.2f} {distance_from_max:<12.2f} "
            f"{range_value:<12.2f}"
        )

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\nShutting down agent on port {args.port}")


if __name__ == "__main__":
    main()
