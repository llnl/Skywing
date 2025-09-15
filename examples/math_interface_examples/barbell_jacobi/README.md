@defgroup barbell Barbell Jacobi
# Barbell Example: Asynchronous Jacobi Method with Skywing

## Overview

This example demonstrates the **Asynchronous Jacobi Method** implemented in the Skywing framework for solving a system of linear equations of the form **Ax = b**. The method is designed for square, strictly diagonally dominant matrices, ensuring a unique solution. Unlike traditional parallel implementations, this approach is fully decentralized, with no manager-slave hierarchy, making it robust to communication delays and flexible in data partitioning.

**What is a Barbell Graph?**
A barbell graph is a graph structure consisting of two complete graphs (clusters where every node is connected to every other node in the cluster) joined by a single path (a sequence of nodes connected in a line). In this example, the Laplacian matrix of a barbell graph is used as the system matrix A. This structure provides a challenging test case for distributed solvers, as it combines densely connected regions with a sparse bridge, highlighting the strengths of asynchronous and decentralized computation.

The example uses a "barbell" graph Laplacian as the system matrix, partitioned across multiple Skywing agents, each running independently and communicating asynchronously.

---

## Features

- **Decentralized Asynchronous Jacobi:** Each agent updates its subset of the solution independently, communicating only with its neighbors.
- **Flexible Partitioning:** Supports arbitrary row partitions across agents.
- **Automated Topology and Data Generation:** Python scripts generate the barbell graph Laplacian, right-hand side, and partitions.
- **Visualization:** Includes scripts for plotting convergence and generating a solution progress video.

## File Formats

- **System Matrix and RHS Vector:** These are generated and stored in plain text format, compatible with the Skywing input routines. They are not strictly in Matrix Market format unless specified by the preprocessing script.
- **Outputs:** Solution vectors and convergence data are written in plain text for post-processing and visualization.


## File Structure

| File/Folder                | Description                                                                 |
|----------------------------|-----------------------------------------------------------------------------|
| `run.sh`                   | Main script to set up, run the example, and generate plots/videos           |
| `jacobi_pre_processing.py` | Python preprocessing script for partitioning and formatting input data       |
| `system/`                  | Directory for generated system files (matrix, RHS, partition info)          |
| `data_hold/`               | Directory for storing output and convergence data                           |


## Inputs & Outputs

### Inputs

- **Row Partition of A**: Submatrix assigned to each agent
- **Row Partition of b**: Corresponding RHS vector segment
- **Row Indices**: Mapping of partitions to global row indices
- **Partition Information:** In this example, we are not using a partition file; instead, each agent is assigned one row of the Laplacian. 

### Outputs

- **Solution Vector x**: Each agent writes its computed solution for its partition
- **Convergence Data**: For plotting and video generation

---

## How to Run

1. **Build the C++ Executable**
   - Ensure all dependencies are installed and the Skywing library is built.
   - Build the Jacobi example executable (update `@barbell_jacobi_exe@` in `run.sh` to your actual binary).

2. **Run the Example**
   ```bash
   ./run.sh [starting_port_number]
   ```
   - Example: `./run.sh 5000`
   - This script will:
     - Clean and create necessary directories
     - Generate the barbell graph Laplacian and partition files
     - Launch the Jacobi solver with one agent per row
     - Plot convergence and generate a solution progress video

3. **View Results**
   - Solution vectors and convergence data are saved in `data_hold/` and `system/`.
   - Visualizations are generated as image/video files.

---

## Implementation Details

- **Decentralized Communication:** Each agent only communicates with its direct neighbors, as determined by the nonzero structure of the Laplacian matrix.
- **Explicit Indexing:** Partitions are explicit; overlapping rows are handled by agents keeping their own updates and using the most recent received values from neighbors.
- **Minimum Agents:** At least 2 agents are required for proper operation.

---

## Customization

- **Change Graph Size:** Adjust `size_of_barbell_1` and `size_of_barbell_2` in `run.sh` to change the size of each "bell" in the barbell graph.
- **Matrix/Vector Generation:** Modify `topology_gen.py` options to use custom RHS or Laplacian adjustments.
- **Partitioning:** Use `jacobi_pre_processing.py` to create custom partitions.

---

## Troubleshooting

- **File Not Found Errors:** Ensure all generated files (`laplacian.txt`, `rhs.txt`, `partition.txt`) exist in the `system/` directory.
- **Port Conflicts:** Make sure the `starting_port_number` is available and not used by other processes.

---
