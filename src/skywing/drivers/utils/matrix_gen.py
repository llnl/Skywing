import argparse
import os
from pathlib import Path

import matplotlib.pyplot as plt
import networkx as nx
import numpy as np


def add_argparse_matrix_options(parser: argparse.ArgumentParser):

    # General matrix options
    matrix_options = parser.add_argument_group("Matrix Options")
    matrix_options.add_argument(
        "--matrix_type",
        type=str,
        choices=["identity", "random", "graph_laplacian", "graph_adjacency", "read"],
        default="random",
        help="Type of matrix to generate. Can also read a matrix and generate appropriate partitioning.",
    )
    matrix_options.add_argument(
        "--rhs",
        type=str,
        choices=["trivial", "uniform", "random", "read"],
        default="trivial",
        help="Option for the right-hand side vector.",
    )
    matrix_options.add_argument(
        "--lam", type=float, default=None, help="l2 regularization."
    )
    matrix_options.add_argument(
        "--n_rows", type=int, default=10, help="Number of rows."
    )
    matrix_options.add_argument(
        "--n_cols",
        type=int,
        default=None,
        help="Number of columns (defaults to number of rows).",
    )
    matrix_options.add_argument(
        "--partitioning",
        type=str,
        choices=["row", "col"],
        default="row",
        help="Choose row or col partitioning for the matrix.",
    )
    # WM: todo - figure out how I want to handle matrix shift/scale
    matrix_options.add_argument(
        "--shift_scale", type=int, default=0, help="whether to shift/scale data matrix"
    )

    # Random matrix options
    matrix_options.add_argument(
        "--condition_number",
        type=float,
        default=1.0,
        help="Condition number for random matrix (default 1.0).",
    )
    matrix_options.add_argument(
        "--scale",
        type=float,
        default=1.0,
        help="Scaling factor for random matrix (default 1.0).",
    )
    matrix_options.add_argument(
        "--rank",
        type=int,
        default=0,
        help="Rank of random matrix (default 0 corresponds to full rank).",
    )

    # Options for reading matrix/rhs from file
    matrix_options.add_argument(
        "--matrix_read_file",
        type=str,
        help="Path to file to read matrix from (only used with matrix_type = read).",
    )
    matrix_options.add_argument(
        "--rhs_read_file",
        type=str,
        help="Path to file to read righ-hand side from (only used with --rhs read).",
    )

    # Graph options
    matrix_options.add_argument(
        "--graph_type",
        type=str,
        choices=["barbell", "path", "cycle", "star", "complete", "grid"],
        default="grid",
        help="Type of graph to generate.",
    )
    matrix_options.add_argument(
        "--adjust_laplacian",
        type=int,
        default=1,
        help="Make the Laplacian matrix diagonally dominant.",
    )
    matrix_options.add_argument(
        "--barbell_size",
        nargs=2,
        type=int,
        default=[5, 5],
        help="Number of nodes in each complete graph for the barbell graph.",
    )
    matrix_options.add_argument(
        "--grid_size",
        nargs="+",
        type=int,
        default=[5, 5],
        help="Number of nodes in each dimension for the grid graph.",
    )


def argparse_matrix_options_to_kwargs(args):
    if not args.n_cols:
        args.n_cols = args.n_rows
    return vars(args)


def generate_matrix(matrix_type, n_rows, n_cols, output_dir, **kwargs):
    """Generate specified matrix.

    Parameters
    ----------
    matrix_type (str): Type of matrix to generate.
    n (int): number of rows of the matrix.
    m (int): number of columns of the matrix.
    output_dir (str): The output directory.
    kwargs: Additional arguments (e.g. specific args for some graph types).

    Returns
    -------
    The requested matrix as np.array.

    """
    # Generate matrix
    if matrix_type == "identity":
        matrix = np.eye(n_rows, M=n_cols)
    elif matrix_type == "random":
        matrix = generate_random(n_rows, n_cols, **kwargs)
    elif "graph" in matrix_type:
        graph_type = kwargs.pop("graph_type", "complete")
        graph = generate_graph(graph_type, n_rows, output_dir, **kwargs)
        if matrix_type == "graph_laplacian":
            matrix = nx.laplacian_matrix(graph).toarray()
            if kwargs.get("adjust_laplacian"):
                matrix = matrix + np.eye(matrix.shape[0])
        elif matrix_type == "graph_adjacency":
            matrix = nx.adjacency_matrix(graph).toarray()
    elif matrix_type == "read":
        matrix_read_file_path = kwargs.get("matrix_read_file")
        if matrix_read_file_path is None:
            raise ValueError("matrix_read_file must be provided for matrix_type='read'")
        matrix_read_file = Path(matrix_read_file_path)
        if matrix_read_file.suffix == ".csv":
            matrix = np.loadtxt(matrix_read_file, delimiter=",")
        else:
            matrix = np.loadtxt(matrix_read_file)
    else:
        raise ValueError(f"Unsupported matrix type: {matrix_type}")

    # Save matrix
    if output_dir is not None:
        np.savetxt(f"{output_dir}/A.txt", matrix, fmt="%.6f")
        print(f"Matrix saved as {output_dir}/A.txt")

    return matrix


def generate_random(n_rows, n_cols, **kwargs):
    """Generate random matrix with specified conditioning"""
    # Rank used for construction
    r = kwargs.get("rank", min(n_rows, n_cols))
    if r < 1 or r > min(n_rows, n_cols):
        r = min(n_rows, n_cols)

    # Orthonormal factors with r columns
    # (QR on m×r and n×r random Gaussians -> Q is orthonormal)
    rng = np.random.default_rng(1)
    Qu, _ = np.linalg.qr(rng.standard_normal((n_rows, r)))
    Qv, _ = np.linalg.qr(rng.standard_normal((n_cols, r)))

    # Log-spaced singular values from 1 down to 1/condition_number
    # length r; enforce exact ends for stability
    condition_number = kwargs.get("condition_number", 10.0)
    svals = np.logspace(0.0, -np.log10(condition_number), num=r)
    if r >= 2:
        svals[0] = 1.0
        svals[-1] = 1.0 / condition_number
    Sigma = np.diag(svals)

    # Construct A = U Σ V^T (n×m) and scale
    scale = kwargs.get("scale", 1.0)
    return scale * Qu @ Sigma @ Qv.T


def barbell_custom_graph(n1, n2):
    """Generate a barbell graph with two complete graphs K_n1 and K_n2
    connected by a path of length 1.

    Parameters
    ----------
    n1 (int): Number of nodes in the first complete graph.
    n2 (int): Number of nodes in the second complete graph.

    Returns
    -------
    Graph: A barbell graph with a single edge between the two complete graphs.

    """
    # Create two complete graphs K_n1 and K_n2
    K_n1 = nx.complete_graph(n1)
    K_n2 = nx.complete_graph(n2)

    # Create the barbell graph
    B = nx.disjoint_union(K_n1, K_n2)  # Combine the two complete graphs
    B.add_edges_from([(n1 - 1, n1)])  # Connect the two graphs

    return B


def generate_graph(graph_type, n, output_dir, **kwargs):
    """Generate a graph based on the specified type and parameters.

    Parameters
    ----------
    graph_type (str): The type of graph to generate (e.g., 'barbell', 'path', 'cycle', 'star').
    n (int): The number of nodes.
    output_dir (str): The output directory.
    kwargs: Additional parameters required for specific graph types.

    Returns
    -------
    Graph: A NetworkX graph object.

    """
    if graph_type == "barbell":
        barbell_size = kwargs.get(
            "barbell_size", [5, 5]
        )  # Default size if not provided
        if barbell_size is not None:
            graph = barbell_custom_graph(barbell_size[0], barbell_size[1])
        else:
            # Fallback to default values
            graph = barbell_custom_graph(5, 5)
    elif graph_type == "path":
        graph = nx.path_graph(n)
    elif graph_type == "cycle":
        graph = nx.cycle_graph(n)
    elif graph_type == "star":
        graph = nx.star_graph(n)
    elif graph_type == "complete":
        graph = nx.complete_graph(n)
    elif graph_type == "grid":
        grid_size = kwargs.get("grid_size")
        graph = nx.grid_graph(dim=grid_size)
    else:
        raise ValueError(f"Unsupported graph type: {graph_type}")

    # Save an image of the graph
    if output_dir is not None:
        plt.figure(figsize=(8, 6))
        nx.draw(
            graph,
            with_labels=True,
            node_color="lightblue",
            edge_color="gray",
            node_size=700,
        )
        plt.savefig(f"{output_dir}/graph.png")
        plt.close()
        print(f"Graph visualization saved as {output_dir}/graph.png")

    return graph


def generate_rhs(rhs_type, n, output_dir, **kwargs):
    """Generate specified right-hand side vector.

    Parameters
    ----------
    rhs_type (str): Type of vector to generate.
    n (int): size of the vector.
    output_dir (str): The output directory.

    Returns
    -------
    The requested vector as np.array.

    """
    if rhs_type == "trivial":
        b = np.zeros(n)
    elif rhs_type == "uniform":
        b = np.ones(n)
    elif rhs_type == "random":
        b = np.random.rand(n)
    elif rhs_type == "read":
        rhs_read_file = kwargs.get("rhs_read_file")
        if rhs_read_file is None:
            raise ValueError("rhs_read_file must be provided for rhs_type='read'")
        b = np.loadtxt(rhs_read_file)
    else:
        raise ValueError("Invalid option for right-hand side: {rhs_type}")

    # Save right-hand side
    if output_dir is not None:
        np.savetxt(f"{output_dir}/b.txt", b, fmt="%.6f")
        print(f"RHS saved as {output_dir}/b.txt")

    return b


def generate_solution(A, b, output_dir, **kwargs):
    """Solve the linear system and save the solution to file.

    Parameters
    ----------
    A (np.array): The matrix.
    b (np.array): The right-hand side vector.
    output_dir (str): The output directory.
    kwargs: Additional arguments (e.g. l2 regularization term, lam).

    """
    # Optional l2 regularization
    lam = kwargs.get("lam")
    if lam is not None:
        A = np.vstack((A, np.sqrt(lam) * np.eye(A.shape[1])))
        b = np.concatenate((b, np.zeros(A.shape[1])))
    x, residuals, rank, singular_values = np.linalg.lstsq(A, b)
    if output_dir is not None:
        np.savetxt(f"{output_dir}/x.txt", x, fmt="%.6f")
        print(f"Solution saved as {output_dir}/x.txt")
    return x


def generate_partitions(matrix, num_agents, partitioning, output_dir):
    """Save a partition of matrix rows and columns.
    NOTE: only support non-overlapping, uniform tiling for now.
    Each line in the file contains the indices for one partition (space-separated).

    Parameters
    ----------
    matrix (np.array): The matrix whose rows/columns are to be partitioned.
    num_agents (int): Number of agents.
    partitioning (str): Row or col partitioning.
    output_dir (str): The output directory.

    """

    row_partitions = 1
    col_partitions = 1
    if partitioning == "row":
        row_partitions = num_agents
    elif partitioning == "col":
        col_partitions = num_agents
    else:
        # WM: todo - implement tiling? Is this useful?
        raise RuntimeError("Partitioning must be 'row' or 'col'.")

    # Partition row indices
    row_indices = [[] for _ in range(row_partitions)]
    for i in range(matrix.shape[0]):
        partition_id = i * row_partitions // matrix.shape[0]
        row_indices[partition_id].append(i)

    # Partition col indices
    col_indices = [[] for _ in range(col_partitions)]
    for i in range(matrix.shape[1]):
        partition_id = i * col_partitions // matrix.shape[1]
        col_indices[partition_id].append(i)

    # Generate row/col partitions for each agent
    num_agents = row_partitions * col_partitions
    row_partition = []
    col_partition = []
    for agent in range(num_agents):
        row_partition.append(row_indices[agent * row_partitions // num_agents])
        col_partition.append(col_indices[agent % col_partitions])

    # Save to file
    if output_dir is not None:
        with open(f"{output_dir}/row_partition.txt", "w") as f:
            for agent in range(num_agents):
                f.write(" ".join([str(i) for i in row_partition[agent]]) + "\n")
        print(f"Row partition saved as {output_dir}/row_partition.txt")
        with open(f"{output_dir}/col_partition.txt", "w") as f:
            for agent in range(num_agents):
                f.write(" ".join([str(i) for i in col_partition[agent]]) + "\n")
        print(f"Column partition saved as {output_dir}/col_partition.txt")

    return row_partition, col_partition


# WM: todo - this is not currently used. Would be a nice feature to add back.
#            I think the way to do this would be to create/modify a config
#            file and explicitly state communication nbrs there.
def generate_matrix_sparsity_comm_topology(
    partitioning, num_agents, matrix, output_dir
):
    """Save the communication topology info.

    Parameters
    ----------
    partitioning (str): The communication topology type.
    output_dir (str): The output directory.
    num_agents (int): The number of agents.
    matrix (np.array): The matrix (used only when building the topology based on the matrix sparsity).

    """
    comm_topology = {i: set() for i in range(num_agents)}

    if num_agents == 1:
        comm_topology[0].add(0)

    # Generate row partition based on matrix sparsity
    if partitioning == "row":
        # WM: note - if we end up supporting more partitioning strategies, we will have to change this
        row_indices = [[] for _ in range(num_agents)]
        for i in range(matrix.shape[0]):
            partition_id = i * num_agents // matrix.shape[0]
            row_indices[partition_id].append(i)

        # Loop over agents
        for agent in range(num_agents):
            # Always add self as a comm neighbor
            comm_topology[agent].add(agent)

            # Get the col indices in the local matrix rows that correspond to neighbor rows
            local_rows = matrix[row_indices[agent], :]
            col_indices = set(np.where(np.abs(local_rows) > 0.0)[1])
            nbr_col_indices = col_indices.difference(set(row_indices[agent]))

            # Loop over the neighbor col indices and add corresponding neighbor agents
            for j in nbr_col_indices:
                for nbr in range(num_agents):
                    if j in row_indices[nbr]:
                        comm_topology[agent].add(nbr)
                        break

    # Generate row partition based on matrix sparsity
    elif partitioning == "col":
        raise RuntimeError(
            "Comm topology based on matrix sparsity is not supported for col partitioning."
        )

    if output_dir is not None:
        with open(f"{output_dir}/comm_topology.txt", "w") as f:
            for agent in range(num_agents):
                f.write(" ".join([str(i) for i in comm_topology[agent]]) + "\n")
        print(f"Communication topology saved as {output_dir}/comm_topology.txt")


def read_partition(filename):
    """Read in partition or comm topology files generated here."""
    partition = []
    with open(filename) as f:
        line = f.readline()
        while line:
            partition.append([])
            for i in line.split():
                partition[-1].append(int(i))
            line = f.readline()
    return partition


def matrix_gen(
    matrix_type="random",
    rhs_type="uniform",
    n_rows=10,
    n_cols=10,
    num_agents=1,
    partitioning="row",
    sparsity_comm_topology=False,
    output_dir=None,
    **kwargs,
):
    """Main function to generate a matrix and save to file.

    Parameters
    ----------
    matrix_type (str): Type of matrix to generate.
    rhs_type (str): Option for the right-hand side vector.
    n_rows (int): number of rows of the matrix.
    n_cols (int): number of columns of the matrix.
    num_agents (int): number of agents for the partitioning.
    partitioning (str): row or col partitioning.
    sparsity_comm_topology (bool): generate comm topology based on matrix sparsity.
    output_dir (str): Output directory. If None, problem is not saved to file.
    kwargs: Additional arguments (e.g. specific args for some graph types).

    """
    if output_dir is not None:
        os.makedirs(output_dir, exist_ok=True)

    # Generate the matrix
    A = generate_matrix(matrix_type, n_rows, n_cols, output_dir, **kwargs)

    # Generate right-hand side vector
    b = generate_rhs(rhs_type, A.shape[0], output_dir, **kwargs)

    # Solve the system and save the solution
    x = generate_solution(A, b, output_dir, **kwargs)

    # Save partitioning info
    row_partition, col_partition = generate_partitions(
        A, num_agents, partitioning, output_dir
    )

    # Save communication topology info
    # WM: todo - figure out how you want to handle sparsity comm topology
    # if sparsity_comm_topology:
    #     generate_matrix_sparsity_comm_topology(
    #         partitioning, num_agents, A, output_dir
    #     )

    return A, b, x, row_partition, col_partition


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run the LinearSystemDriver.")

    # General options
    parser.add_argument(
        "--output_dir",
        type=Path,
        default=Path("matrix_gen_output"),
        help="Output directory where the problem info is saved.",
    )

    # Matrix generation arguments
    add_argparse_matrix_options(parser)

    args = parser.parse_args()

    kwargs = argparse_matrix_options_to_kwargs(args)

    matrix_gen(**kwargs)
