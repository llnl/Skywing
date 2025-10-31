import os
from pathlib import Path
import argparse
import numpy as np
import networkx as nx
import matplotlib.pyplot as plt

def generate_matrix(matrix_type, n, m, output_dir, **kwargs):
    """
    Generate specified matrix.

    Parameters:
    matrix_type (str): Type of matrix to generate.
    n (int): number of rows of the matrix.
    m (int): number of columns of the matrix.
    output_dir (str): The output directory.
    kwargs: Additional arguments (e.g. specific args for some graph types).

    Returns:
    The requested matrix as np.array.
    """

    # Generate matrix
    if matrix_type == 'identity':
        matrix = np.eye(n, M=m)
    elif matrix_type == 'random':
        matrix = generate_random(n, m, **kwargs)
    elif 'graph' in matrix_type:
        graph_type = kwargs.pop('graph_type', 'complete')
        graph = generate_graph(graph_type, n, output_dir, **kwargs)
        if matrix_type == 'graph_laplacian':
            matrix = nx.laplacian_matrix(graph).toarray()
            if kwargs.get('adjust_laplacian'):
                matrix = matrix + np.eye(matrix.shape[0])
        elif matrix_type == 'graph_adjacency':
            matrix = nx.adjacency_matrix(graph).toarray()
    elif matrix_type == 'read':
        matrix_read_file = Path(kwargs.get('matrix_read_file'))
        if matrix_read_file.suffix == '.csv':
            matrix = np.loadtxt(matrix_read_file, delimiter=',')
        else:
            matrix = np.loadtxt(matrix_read_file)
    else:
        raise ValueError(f"Unsupported matrix type: {matrix_type}")

    # Save matrix
    np.savetxt(f"{output_dir}/A.txt", matrix, fmt='%.6f')
    print(f"Matrix saved as {output_dir}/A.txt")
    return matrix

def generate_random(n, m, **kwargs):
    """
    Generate random matrix with specified conditioning
    """

    # Rank used for construction
    r = kwargs.get('rank', min(n,m))
    if r < 1 or r > min(n,m):
        r = min(n,m)

    # Orthonormal factors with r columns
    # (QR on m×r and n×r random Gaussians -> Q is orthonormal)
    rng = np.random.default_rng(1)
    Qu, _ = np.linalg.qr(rng.standard_normal((n, r)))
    Qv, _ = np.linalg.qr(rng.standard_normal((m, r)))

    # Log-spaced singular values from 1 down to 1/condition_number
    # length r; enforce exact ends for stability
    condition_number = kwargs.get('condition_number', 10.0)
    svals = np.logspace(0.0, -np.log10(condition_number), num=r)
    if r >= 2:
        svals[0] = 1.0
        svals[-1] = 1.0 / condition_number
    Sigma = np.diag(svals)

    # Construct A = U Σ V^T (n×m) and scale
    scale = kwargs.get('scale', 1.0)
    return scale * Qu @ Sigma @ Qv.T

def barbell_custom_graph(n1, n2):
    """
    Generate a barbell graph with two complete graphs K_n1 and K_n2
    connected by a path of length 1.

    Parameters:
    n1 (int): Number of nodes in the first complete graph.
    n2 (int): Number of nodes in the second complete graph.

    Returns:
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
    """
    Generate a graph based on the specified type and parameters.

    Parameters:
    graph_type (str): The type of graph to generate (e.g., 'barbell', 'path', 'cycle', 'star').
    n (int): The number of nodes.
    output_dir (str): The output directory.
    kwargs: Additional parameters required for specific graph types.

    Returns:
    Graph: A NetworkX graph object.
    """
    if graph_type == 'barbell':
        barbell_size = kwargs.get('barbell_size')
        graph = barbell_custom_graph(barbell_size[0], barbell_size[1])
    elif graph_type == 'path':
        graph = nx.path_graph(n)
    elif graph_type == 'cycle':
        graph = nx.cycle_graph(n)
    elif graph_type == 'star':
        graph = nx.star_graph(n)
    elif graph_type == 'complete':
        graph = nx.complete_graph(n)
    elif graph_type == 'grid':
        grid_size = kwargs.get('grid_size')
        graph = nx.grid_graph(dim = grid_size)
    else:
        raise ValueError(f"Unsupported graph type: {graph_type}")

    # Save an image of the graph
    plt.figure(figsize=(8, 6))
    nx.draw(graph, with_labels=True, node_color='lightblue', edge_color='gray', node_size=700)
    plt.savefig(f'{output_dir}/graph.png')
    plt.close()
    print(f"Graph visualization saved as {output_dir}/graph.png")

    return graph

def generate_rhs(rhs_type, n, output_dir, **kwargs):
    """
    Generate specified right-hand side vector.

    Parameters:
    rhs_type (str): Type of vector to generate.
    n (int): size of the vector.
    output_dir (str): The output directory.

    Returns:
    The requested vector as np.array.
    """

    if rhs_type == 'trivial':
        b = np.zeros(n)
    elif rhs_type == 'uniform':
        b = np.ones(n)
    elif rhs_type == 'random':
        b = np.random.rand(n)
    elif rhs_type == 'read':
        b = np.loadtxt(kwargs.get('rhs_read_file'))
    else:
        raise ValueError("Invalid option for right-hand side: {rhs_type}")

    # Save right-hand side
    np.savetxt(f"{output_dir}/b.txt", b, fmt='%.6f')
    print(f"RHS saved as {output_dir}/b.txt")
    return b

def generate_solution(A, b, output_dir, **kwargs):
    """
    Solve the linear system and save the solution to file.

    Parameters:
    A (np.array): The matrix.
    b (np.array): The right-hand side vector.
    output_dir (str): The output directory.
    kwargs: Additional arguments (e.g. l2 regularization term, lam).
    """

    # Optional l2 regularization
    if kwargs.get('lam'):
        lam = kwargs.get('lam')
        A = np.vstack((A, np.sqrt(lam) * np.eye(A.shape[1])))
        b = np.concatenate((b, np.zeros(A.shape[1])))
    x, residuals, rank, singular_values = np.linalg.lstsq(A, b)
    np.savetxt(f"{output_dir}/x.txt", x, fmt='%.6f')
    print(f"Solution saved as {output_dir}/x.txt")

def generate_partitions(matrix, output_dir, row_partitions, col_partitions):
    """
    Save a partition of matrix rows and columns.
    NOTE: only support non-overlapping, uniform tiling for now.
    Each line in the file contains the indices for one partition (space-separated).

    Parameters:
    matrix (np.array): The matrix whose rows/columns are to be partitioned.
    output_dir (str): The output directory.
    row_partitions (int): Number of row partitions.
    col_partitions (int): Number of column partitions.
    """

    # Generate row partition
    row_indices = [[] for _ in range(row_partitions)]
    for i in range(matrix.shape[0]):
        partition_id = i * row_partitions // matrix.shape[0]
        row_indices[partition_id].append(str(i))

    # Generate col partition
    col_indices = [[] for _ in range(col_partitions)]
    for i in range(matrix.shape[1]):
        partition_id = i * col_partitions // matrix.shape[1]
        col_indices[partition_id].append(str(i))

    # Save to file
    num_agents = row_partitions * col_partitions
    with open(f'{output_dir}/row_partition.txt', 'w') as f:
        for agent in range(num_agents):
            f.write(' '.join(row_indices[agent * row_partitions // num_agents]) + '\n')
    print(f"Row partition saved as {output_dir}/row_partition.txt")
    with open(f'{output_dir}/col_partition.txt', 'w') as f:
        for agent in range(num_agents):
            f.write(' '.join(col_indices[agent % col_partitions]) + '\n')
    print(f"Column partition saved as {output_dir}/col_partition.txt")

def generate_comm_topology(comm_topology, output_dir, num_agents, matrix):
    """
    Save the communication topology info.

    Parameters:
    comm_topology (str): The communication topology type.
    output_dir (str): The output directory.
    num_agents (int): The number of agents.
    matrix (np.array): The matrix (used only when building the topology based on the matrix sparsity).
    """

    comm_nbrs = { i : set() for i in range(num_agents) }

    if num_agents == 1:
        comm_nbrs[0].add(0)

    else:
        # Matrix sparsity
        if comm_topology == 'sparsity':

            # Generate row partition
            # WM: note - if we end up supporting more partitioning strategies, we will have to change this
            row_indices = [[] for _ in range(num_agents)]
            for i in range(matrix.shape[0]):
                partition_id = i * num_agents // matrix.shape[0]
                row_indices[partition_id].append(i)

            # Loop over agents
            for agent in range(num_agents):

                # Always add self as a comm neighbor
                comm_nbrs[agent].add(agent)

                # Get the col indices in the local matrix rows that correspond to neighbor rows
                local_rows = matrix[row_indices[agent],:]
                col_indices = set(np.where(np.abs(local_rows) > 0.0)[1])
                nbr_col_indices = col_indices.difference(set(row_indices[agent]))

                # Loop over the neighbor col indices and add corresponding neighbor agents
                for j in nbr_col_indices:
                    for nbr in range(num_agents):
                        if j in row_indices[nbr]:
                            comm_nbrs[agent].add(nbr)
                            break

        # Fully connected
        elif comm_topology == 'full':
            for agent in range(num_agents):
                for nbr in range(num_agents):
                    comm_nbrs[agent].add(nbr)

        # Ring topology
        elif comm_topology == 'ring':
            comm_nbrs[0].add(num_agents - 1)
            comm_nbrs[0].add(0)
            comm_nbrs[0].add(1)
            for agent in range(1,num_agents - 1):
                comm_nbrs[agent].add(agent - 1)
                comm_nbrs[agent].add(agent)
                comm_nbrs[agent].add(agent + 1)
            comm_nbrs[num_agents - 1].add(num_agents - 2)
            comm_nbrs[num_agents - 1].add(num_agents - 1)
            comm_nbrs[num_agents - 1].add(0)

        # Line topology
        elif comm_topology == 'line':
            comm_nbrs[0].add(0)
            comm_nbrs[0].add(1)
            for agent in range(1,num_agents - 1):
                comm_nbrs[agent].add(agent - 1)
                comm_nbrs[agent].add(agent)
                comm_nbrs[agent].add(agent + 1)
            comm_nbrs[num_agents - 1].add(num_agents - 2)
            comm_nbrs[num_agents - 1].add(num_agents - 1)

    with open(f'{output_dir}/comm_topology.txt', 'w') as f:
        for agent in range(num_agents):
            f.write(' '.join([str(i) for i in comm_nbrs[agent]]) + '\n')
    print(f"Communication topology saved as {output_dir}/comm_topology.txt")

def main(matrix_type, output_dir, rhs_type, n, m, row_partitions, col_partitions, comm_topology, **kwargs):
    """
    Main function to generate a matrix and save to file.

    Parameters:
    matrix_type (str): Type of matrix to generate.
    output_dir (str): Output directory.
    rhs_type (str): Option for the right-hand side vector ('trivial', 'uniform', 'random').
    n (int): number of rows of the matrix.
    m (int): number of columns of the matrix.
    row_partitions (int): number of partitions to divide rows into.
    col_partitions (int): number of partitions to divide columns into.
    comm_topology (str): communication topology type.
    kwargs: Additional arguments (e.g. specific args for some graph types).
    """
    os.makedirs(output_dir, exist_ok=True)

    # Generate the matrix
    A = generate_matrix(matrix_type, n, m, output_dir, **kwargs)

    # Generate right-hand side vector
    b = generate_rhs(rhs_type, A.shape[0], output_dir, **kwargs)

    # Solve the system and save the solution
    generate_solution(A, b, output_dir, **kwargs)

    # Save partitioning info
    generate_partitions(A, output_dir, row_partitions, col_partitions)

    # Save communication topology info
    generate_comm_topology(comm_topology, output_dir, row_partitions * col_partitions, A)


if __name__ == "__main__":
    import argparse

    # Parse arguments
    parser = argparse.ArgumentParser(description='Generate simple matrices.')
    parser.add_argument('matrix_type', type=str, choices=['identity', 'random', 'graph_laplacian', 'graph_adjacency', 'read'],
                        help='Type of matrix to generate. Can also read a matrix and generate appropriate partitioning and communication topology info.')
    parser.add_argument('-o', '--output_dir', type=str, default='data',
                        help='Output directory where the matrix and related files are saved.')
    parser.add_argument('-b', '--rhs', type=str, choices=['trivial', 'uniform', 'random', 'read'], default='trivial',
                        help='Option for the right-hand side vector.')
    parser.add_argument('-l', '--lam', type=float, default=None, help='l2 regularization.')
    parser.add_argument('-n', '--n', type=int, default=10, help='Number of rows.')
    parser.add_argument('-m', '--m', type=int, default=None, help='Number of columns (defaults to number of rows).')
    parser.add_argument('-r', '--row_partitions', type=int, default=1, help='Number of row partitions.')
    parser.add_argument('-c', '--col_partitions', type=int, default=1, help='Number of column partitions.')
    parser.add_argument('-t', '--comm_topology', type=str, choices=['sparsity', 'full', 'ring', 'line'],
                        default='sparsity', help='Communication topology.')

    # Random matrix options
    parser.add_argument('-k', '--condition_number', type=float, default=1.0, help='Condition number for random matrix (default 1.0).')
    parser.add_argument('-s', '--scale', type=float, default=1.0, help='Scaling factor for random matrix (default 1.0).')
    parser.add_argument('-R', '--rank', type=int, default=0, help='Rank of random matrix (default 0 corresponds to full rank).')

    # Options for reading matrix/rhs from file
    parser.add_argument('--matrix_read_file', type=str, help='Path to file to read matrix from (only used with matrix_type = read).')
    parser.add_argument('--rhs_read_file', type=str, help='Path to file to read righ-hand side from (only used with --rhs read).')

    # Graph options
    parser.add_argument('-g', '--graph_type', type=str, choices=['barbell', 'path', 'cycle', 'star', 'complete', 'grid'],
                        help='Type of graph to generate.')
    parser.add_argument('--adjust_laplacian', action='store_true', help='Make the Laplacian matrix diagonally dominant.')
    parser.add_argument('--barbell_size', nargs=2, type=int, default=[5,5],
                        help='Number of nodes in each complete graph for the barbell graph.')
    parser.add_argument('--grid_size', nargs='+', type=int, default=[5,5],
                        help='Number of nodes in each dimension for the grid graph.')

    args = parser.parse_args()

    if not args.m:
        args.m = args.n

    main(args.matrix_type,
         args.output_dir,
         args.rhs,
         args.n,
         args.m,
         args.row_partitions,
         args.col_partitions,
         args.comm_topology,
         lam=args.lam,
         condition_number=args.condition_number,
         scale=args.scale,
         rank=args.rank,
         matrix_read_file=args.matrix_read_file,
         rhs_read_file=args.rhs_read_file,
         graph_type=args.graph_type,
         adjust_laplacian=args.adjust_laplacian,
         barbell_size=args.barbell_size,
         grid_size=args.grid_size)
