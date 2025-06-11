import os
import networkx as nx
import matplotlib.pyplot as plt
import numpy as np

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
    B.add_edges_from([(n1 - 1, n1), (n1 + n2 - 1, n1 + n2-1)])  # Connect the two graphs
    
    return B

def generate_graph(graph_type, **kwargs):
    """
    Generate a graph based on the specified type and parameters.

    Parameters:
    graph_type (str): The type of graph to generate (e.g., 'barbell', 'path', 'cycle', 'star').
    kwargs: Additional parameters required for specific graph types.

    Returns:
    Graph: A NetworkX graph object.
    """
    if graph_type == 'barbell':
        n1 = kwargs.get('n1', 5)
        n2 = kwargs.get('n2', 5)
        return barbell_custom_graph(n1, n2)
    elif graph_type == 'path':
        n = kwargs.get('n', 10)
        return nx.path_graph(n)
    elif graph_type == 'cycle':
        n = kwargs.get('n', 10)
        return nx.cycle_graph(n)
    elif graph_type == 'star':
        n = kwargs.get('n', 10)
        return nx.star_graph(n)
    elif graph_type == 'complete':
        n = kwargs.get('n', 10)
        return nx.complete_graph(n)
    elif graph_type == 'grid':
        m = kwargs.get('m', 5)
        n = kwargs.get('n', 5)
        return nx.grid_2d_graph(m, n)
    else:
        raise ValueError(f"Unsupported graph type: {graph_type}")


def save_dense_matrix(matrix, filename):
    """ Save a dense matrix to a text file. """
    np.savetxt(f"{filename}.txt", matrix, fmt='%.6f')
    print(f"Matrix saved as {filename}.txt")


def save_vector(vector, filename):
    """ Save a vector to a text file. """
    with open(f"{filename}.txt", 'w') as f:
        for element in vector:
            f.write(f"{element}\n")
    print(f"Vector saved as {filename}.txt")


def save_adjacency_matrix(graph, filename):
    """ Save the adjacency matrix of the graph to a dense text file. """
    adjacency_matrix = nx.to_numpy_array(graph)
    save_dense_matrix(adjacency_matrix, filename)


def save_laplacian_matrix(graph, filename, adjust_laplacian=False):
    """
    Save the Laplacian matrix of the graph to a dense text file.

    Parameters:
    graph (Graph): The graph for which to save the Laplacian matrix.
    filename (str): The filename to save the Laplacian matrix.
    adjust_laplacian (bool): Whether to make the Laplacian matrix diagonally dominant.
    """
    laplacian_matrix = nx.laplacian_matrix(graph).toarray()
    if adjust_laplacian:
        laplacian_matrix = laplacian_matrix + laplacian_matrix.shape[0] * np.eye(laplacian_matrix.shape[0])
    save_dense_matrix(laplacian_matrix, filename)
    return laplacian_matrix


def save_rhs(rhs, filename):
    """ Save the right-hand side vector to a text file. """
    if rhs.ndim != 1:
        raise ValueError("Input rhs must be a 1D vector.")
    save_vector(rhs, filename)


def save_solution(solution, filename):
    """ Save the solution vector to a text file. """
    if solution.ndim != 1:
        raise ValueError("Input solution must be a 1D vector.")
    save_vector(solution, filename)


def save_partition(graph, filename, num_partitions=1):
    """
    Save a partition of the graph nodes into num_partitions groups.
    Each line in the file contains the node indices for one partition (space-separated).
    
    Parameters:
    graph (Graph): The graph whose nodes are to be partitioned.
    filename (str): The output filename.
    num_partitions (int): Number of partitions to divide the nodes into.
    """
    num_nodes = graph.number_of_nodes()
    # Assign nodes to partitions (round-robin for simplicity)
    partitions = [[] for _ in range(num_partitions)]
    for node in range(num_nodes):
        partition_id = node % num_partitions
        partitions[partition_id].append(str(node))

    with open(filename, 'w') as f:
        for partition in partitions:
            f.write(' '.join(partition) + '\n')
    print(f"Partition into {num_partitions} groups saved as {filename}")

    
def visualize_graph(graph, filename):
    """ Visualize and save the graph as an image. """
    plt.figure(figsize=(8, 6))
    nx.draw(graph, with_labels=True, node_color='lightblue', edge_color='gray', node_size=700)
    plt.title(f'Graph Visualization ({filename})')
    plt.savefig(f'{filename}.png')
    plt.close()
    print(f"Graph visualization saved as {filename}.png")


def main(graph_type, path, rhs_option, adjust_laplacian=False, num_partitions=1, **kwargs):
    """
    Main function to generate a graph, save its properties, and solve a system.

    Parameters:
    graph_type (str): Type of graph to generate.
    path (str): Directory path to save the files.
    rhs_option (str): Option for the right-hand side vector ('trivial', 'uniform', 'random').
    adjust_laplacian (bool): Whether to make the Laplacian matrix diagonally dominant.
    kwargs: Additional parameters for graph generation.
    """
    os.makedirs(path, exist_ok=True)

    # Generate the graph
    graph = generate_graph(graph_type, **kwargs)

    # Visualize and save the graph
    visualize_graph(graph, f'{path}/graph')

    # Save adjacency matrix
    save_adjacency_matrix(graph, f'{path}/adjacency')

    # Save Laplacian matrix
    laplacian_matrix = save_laplacian_matrix(graph, f'{path}/laplacian', adjust_laplacian)

    # Generate right-hand side vector
    num_nodes = graph.number_of_nodes()
    if rhs_option == 'trivial':
        rhs = np.zeros(num_nodes)
    elif rhs_option == 'uniform':
        rhs = np.ones(num_nodes)
    elif rhs_option == 'random':
        rhs = np.random.rand(num_nodes)
    else:
        raise ValueError("Invalid option for right-hand side. Choose 'trivial', 'uniform', or 'random'.")

    # Save right-hand side vector
    save_rhs(rhs, f'{path}/rhs')

    # Save partition
    save_partition(graph, f'{path}/partition.txt', num_partitions)

    # Solve the system and save the solution
    solution = np.linalg.solve(laplacian_matrix, rhs)
    save_solution(solution, f'{path}/solution')


if __name__ == "__main__":
    import argparse

    # Set up argument parsing
    parser = argparse.ArgumentParser(description='Generate and analyze graphs.')
    parser.add_argument('graph_type', type=str, choices=['barbell', 'path', 'cycle', 'star', 'complete', 'grid'], 
                        help='Type of graph to generate.')
    parser.add_argument('--path', type=str, default='data', help='Directory path to save the files.')
    parser.add_argument('--rhs', type=str, choices=['trivial', 'uniform', 'random'], default='trivial', 
                        help='Option for the right-hand side vector: "trivial", "uniform", or "random".')
    parser.add_argument('--adjust_laplacian', action='store_true', 
                        help='Make the Laplacian matrix diagonally dominant.')
    parser.add_argument('--num_partitions', type=int, default=1, help='Number of partitions for the partition file.')

    parser.add_argument('--n1', type=int, default=5, help='Number of nodes in the first complete graph (for barbell).')
    parser.add_argument('--n2', type=int, default=5, help='Number of nodes in the second complete graph (for barbell).')
    parser.add_argument('--n', type=int, default=10, help='Number of nodes (for path, cycle, star, complete graphs).')
    parser.add_argument('--m', type=int, default=5, help='Number of rows (for grid graph).')

    args = parser.parse_args()
    print(args.num_partitions)
    # Generate the graph and analyze it
    main(args.graph_type, args.path, args.rhs, adjust_laplacian=args.adjust_laplacian, n1=args.n1, n2=args.n2, n=args.n, m=args.m, num_partitions=args.num_partitions)