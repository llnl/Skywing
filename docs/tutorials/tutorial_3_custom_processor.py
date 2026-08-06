"""
Tutorial 3: Building a Custom Processor - Jacobi Linear Solver
Implementing a distributed iterative method for solving linear systems.

This tutorial demonstrates how to create a custom processor by implementing
the Jacobi iteration method for solving Ax = b.

Usage:
    python docs/tutorials/tutorial_3_custom_processor.py --port 20000 --nbr-ports 20001,20002 --agent-id 0
"""

import argparse
import os
import time

import numpy as np

from skywing.core import Agent
from skywing.core.types import ProcessorData
from skywing.mid import Iteration
from skywing.mid.base_processor import Processor


class SimpleJacobiData(ProcessorData):
    values: np.ndarray
    partition: list[int]


class SimpleJacobiProcessor(Processor):
    """
    Solves a linear system Ax = b using Jacobi iteration.

    Each agent owns one or more rows of the system and iteratively
    updates its variables using the Jacobi method:
        x_i^(k+1) = (b_i - sum(A_ij * x_j^(k) for j != i)) / A_ii

    In this tutorial, we solve a simple 3x3 system:
        4x + y + z = 9
        x + 4y + z = 12
        x + y + 4z = 15

    Solution: x=1, y=2, z=3
    """

    def __init__(self, data, **kwargs):
        """
        Initialize the Jacobi processor.

        Args:
            data: Tuple of (A_local, b_local) where:
                  - A_local: This agent's rows of the matrix (numpy array)
                  - b_local: This agent's right-hand side values
            **kwargs: Must include 'row_partition' - global indices of local rows
        """
        super().__init__(data, **kwargs)

        A_local, b_local = self.data
        self.A_local = A_local
        self.b_local = b_local

        # Get row partition (which rows this agent owns)
        self.row_partition = self.parameters["row_partition"]

        # Total size of the system (number of columns in A)
        self.n = A_local.shape[1]

        # Current solution vector (all agents need full x vector)
        self.x_global = np.zeros(self.n)

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
        D_inv = np.diag(
            1.0 / self.A_local[np.arange(len(self.row_partition)), self.row_partition]
        )

        # Compute residual and update
        residual = self.b_local - self.A_local @ self.x_global
        self.x_global[self.row_partition] += D_inv @ residual

        # Update result
        self.result = self.x_global

    def prepare_for_publication(self):
        """Return data to send to neighbors."""
        return SimpleJacobiData(
            values=self.x_global[self.row_partition], partition=self.row_partition
        )

    def convert(self, publish_data):
        """Convert data to serializable format."""
        x_values, indices = publish_data
        return [], list(x_values), list(indices)

    def deconvert(self, recv_strings, recv_doubles, recv_ints):
        """Convert received data back to original format."""
        return (np.array(recv_doubles), recv_ints)


def main():
    parser = argparse.ArgumentParser(
        description="Tutorial 3: Custom Jacobi processor for distributed linear solving"
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
        "--agent-id",
        type=int,
        required=True,
        help="Agent ID (0, 1, or 2) - determines which equation this agent owns",
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

    # Load matrix data from files
    # Get the directory where this script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    data_dir = os.path.join(script_dir, "jacobi_data")

    if args.print_header:
        print(f"Loading matrix data from: {data_dir}")

    # Use LinearSystemDataLoader to load the problem data
    try:
        from skywing.drivers.utils.data_loaders import LinearSystemDataLoader

        data_loader = LinearSystemDataLoader(data_dir, args.agent_id)
        A_local, b_local = data_loader()
        row_partition, col_partition = data_loader.get_partitions()

        # Get this agent's row partition
        my_rows = row_partition[args.agent_id]

    except FileNotFoundError:
        print(f"Error: Could not find matrix data files in {data_dir}")
        print("Please ensure the following files exist:")
        print("  - A.txt (matrix)")
        print("  - b.txt (right-hand side)")
        print("  - row_partition.txt")
        print("  - col_partition.txt")
        return
    except ImportError:
        print("Error: Could not import LinearSystemDataLoader")
        print("Please ensure skywing drivers are installed")
        return

    # Variable names for display
    variable_names = ["x", "y", "z"]

    # Expected solution (for this simple system: x=1, y=2, z=3)
    expected_solution = [1.0, 2.0, 3.0]

    if args.print_header:
        print(f"Starting agent on {address}:{args.port}")
        print(f"Agent ID: {args.agent_id}")
        print(f"Rows owned: {my_rows}")
        print(f"Variables: {[variable_names[i] for i in my_rows]}")
        print(f"Local matrix shape: {A_local.shape}")
        print(f"Neighbors: {neighbor_ports}")

    # Create agent
    agent = Agent(address, args.port)
    neighbors = [(address, p) for p in neighbor_ports]
    agent.configure_neighbors(neighbors)

    # Create custom Jacobi processor with loaded data
    processor = SimpleJacobiProcessor((A_local, b_local), row_partition=my_rows)

    # Create and launch iteration
    iteration = Iteration(processor, agent)
    iteration.launch()

    # Wait for initial convergence
    time.sleep(1.0)

    # Query results multiple times to show convergence
    # Show full solution vector (all three variables) for all agents
    if args.print_header:
        print(f"\nAgent {args.agent_id} - Convergence to Full Solution:")
        print(f"{'Agent':<7} {'Iteration':<12} {'x':<12} {'y':<12} {'z':<12}")
        print("-" * 56)

    for i in range(10):
        time.sleep(0.5)
        result = iteration.query()
        if isinstance(result, np.ndarray) and len(result) == 3:
            # Display all three components with agent ID in each line
            print(
                f"{args.agent_id:<7} {i:<12} {result[0]:<12.6f} {result[1]:<12.6f} {result[2]:<12.6f}"
            )
        else:
            print(f"Warning: Unexpected result format: {result}")

    # Final result - now the full solution vector
    final_result = iteration.query()
    if not isinstance(final_result, np.ndarray):
        final_result = np.array([final_result])

    # Compute errors for all three variables
    errors = np.abs(final_result - expected_solution)

    if args.print_header:
        print("\n" + "=" * 50)
        print(f"Agent {args.agent_id} - Final Results (Full Solution):")
        for i in range(len(expected_solution)):
            var_name = variable_names[i]
            print(
                f"  {var_name}: computed={final_result[i]:.6f}, "
                f"expected={expected_solution[i]:.6f}, error={errors[i]:.2e}"
            )
        print("=" * 50)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\nShutting down agent on port {args.port}")


if __name__ == "__main__":
    main()
