"""
Tutorial 1: Your First Distributed Computation
Finding the maximum value across a network of agents.

This script runs a single agent. Launch multiple instances in separate terminals
or use the provided run_tutorial_1.sh script.

Usage:
    python docs/tutorials/tutorial_1_max.py --port 20000 --nbr-ports 20001,20002 --local-value 42
"""

import argparse
import time

from skywing.core import Agent
from skywing.mid import Iteration, MaxProcessor


def main():
    parser = argparse.ArgumentParser(
        description="Tutorial 1: Maximum consensus across distributed agents"
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

    # Parse port and neighbor ports
    neighbor_ports = [int(p.strip()) for p in args.nbr_ports.split(",")]
    port = args.port

    # Fixed address for simplicity
    address = "127.0.0.1"

    # Create agent
    if args.print_header:
        print(f"Starting agent on {address}:{args.port}")
        print(f"Local value: {args.local_value}")
        print(f"Neighbors: {neighbor_ports}")

    agent = Agent(address, port)

    # Configure neighbors
    neighbors = [(address, p) for p in neighbor_ports]
    agent.configure_neighbors(neighbors)

    # Create processor with local value
    processor = MaxProcessor(args.local_value)

    # Create and launch iteration
    iteration = Iteration(processor, agent)
    iteration.launch()

    # Query the result every second for 10 seconds to see convergence
    if args.print_header:
        print(f"\n{'Port':<10}  {'Local Value':<15}  {'Time':<10}  {'Max Value':<15}")
        print("-" * 70)
    for i in range(10):
        result = iteration.query()
        print(f"{args.port:<10}  {args.local_value:<15}  {f'{i}s':<10}  {result:<15}")
        time.sleep(1)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\nShutting down agent on port {args.port}")


if __name__ == "__main__":
    main()
