import ast
from abc import ABC, abstractmethod
from typing import Any

import numpy as np
import pandas as pd

from skywing.drivers.utils.matrix_gen import read_partition


class DataLoader(ABC):
    """Abstract base class for data loaders."""

    @abstractmethod
    def __call__(self) -> Any:
        """Return the next value."""
        raise NotImplementedError


class RandomScalarLoader(DataLoader):
    """Generates a random scalar."""

    def __init__(self):
        pass

    def __call__(self):
        return np.random.rand()


class RandomVectorLoader(DataLoader):
    """Generates a random vector."""

    def __init__(self, n: int):
        self.n = n

    def __call__(self):
        return np.random.rand(self.n)


class RandomMatrixLoader(DataLoader):
    """Generates a random matrix."""

    def __init__(self, m: int, n: int):
        self.m = m
        self.n = n

    def __call__(self):
        return np.random.rand(self.m, self.n)


class CSVDataLoader(DataLoader):
    """Reads data from csv files
    Each column of the csv file is expected to correspond to an
    agent, indexed from 0. Each row represents a single point in
    a time series for all agents. Data may be scalar, vector, or
    matrix valued and is returned as scalars or numpy arrays.

    csv files with scalar data should have the form:
        4.1; 4.2; 4.3
        3.1; 3.2; 3.3
        2.1; 2.2; 2.3
        1.1; 1.2; 1.3

    csv files with vector data should have the form:
        [4.1, 4.2, 4.3]; [4.2, 4.3, 4.1]; [4.3, 4.1, 4.2]
        [3.1, 3.2, 3.3]; [3.2, 3.3, 3.1]; [3.3, 3.1, 3.2]
        [2.1, 2.2, 2.3]; [2.2, 2.3, 2.1]; [2.3, 2.1, 2.2]
        [1.1, 1.2, 1.3]; [1.2, 1.3, 1.1]; [1.3, 1.1, 1.2]

    csv files with matrix data should have the form:
        [[4.1, 4.2], [4.3, 4.4]]; [[4.4, 4.1], [4.2, 4.3]]; [[4.3, 4.4], [4.1, 4.2]]
        [[3.1, 3.2], [3.3, 3.4]]; [[3.4, 3.1], [3.2, 3.3]]; [[3.3, 3.4], [3.1, 3.2]]
        [[2.1, 2.2], [2.3, 2.4]]; [[2.4, 2.1], [2.2, 2.3]]; [[2.3, 2.4], [2.1, 2.2]]
        [[1.1, 1.2], [1.3, 1.4]]; [[1.4, 1.1], [1.2, 1.3]]; [[1.3, 1.4], [1.1, 1.2]]
    """

    def __init__(self, csv_path: str, agent_number: int):
        """csv_path: path to the .csv file
        agent_number: index of the column to read (0-based)
        """

        # Read csv file to dataframe
        self.df = pd.read_csv(csv_path, header=None, delimiter=";")

        # If df has string values, convert to numpy array
        if isinstance(self.df[0][0], str):

            def str_to_numpy_map(s: str):
                return np.array(ast.literal_eval(s.strip()))

            self.df = self.df.map(str_to_numpy_map)

        # Validate column index
        if agent_number < 0 or agent_number >= self.df.shape[1]:
            raise ValueError(
                f"agent_number {agent_number} is out of range for "
                f"{self.df.shape[1]} columns"
            )

        self.agent_number = agent_number
        self.current_index = 0
        self.is_finished = False

        # For convenience, store just the selected column as a Series
        self.column = self.df.iloc[:, agent_number]

    def __call__(self):
        """Return the next element from the chosen column, repeating the last element in the column when you reach the end."""
        if len(self.column) == 0:
            raise ValueError("The selected column is empty.")

        value = self.column.iloc[self.current_index]
        self.is_finished = self.current_index == len(self.column) - 1
        self.current_index = min((self.current_index + 1), len(self.column) - 1)
        return value

    def get_global_data(self):
        """Return a multi-dimensional numpy array with all the data.
        In the case of scalar data, the returned array has shape:
        (num_rows, num_agents)
        In the case of vector data, the returned array has shape:
        (num_rows, num_agents, vector_size)
        And in the case of matrix data, the returned array has shape:
        (num_rows, num_arrays, matrix_shape[0], matrix_shape[1])
        """
        return np.stack(self.df.values.tolist())


class LinearSystemDataLoader(DataLoader):
    def __init__(self, output_dir: str, agent_id: int):
        """output_dir: directory containing A.txt, b.txt, x.txt, row_partition.txt, col_partition.txt, comm_topology.txt
        agent_id: integer id of the agent
        """
        self.output_dir = output_dir
        self.agent_id = agent_id

        # Placeholders; will be filled by _load_and_build_local()
        self.A = None
        self.b = None
        self.x = None
        self.row_partition = None
        self.col_partition = None
        self.comm_topology = None

        self.A_local = None
        self.b_local = None

        self._load_and_build_local()

    def _load_and_build_local(self):
        """Load global data and construct local problem for this agent."""
        # Read global problem info
        self.A = np.loadtxt(f"{self.output_dir}/A.txt")
        self.b = np.loadtxt(f"{self.output_dir}/b.txt")
        self.row_partition = read_partition(f"{self.output_dir}/row_partition.txt")
        self.col_partition = read_partition(f"{self.output_dir}/col_partition.txt")

        if self.agent_id < 0 or self.agent_id >= len(self.row_partition):
            raise ValueError(f"agent_id {self.agent_id} out of range for row_partition")

        rows = self.row_partition[self.agent_id]
        cols = self.col_partition[self.agent_id]

        # Get the local problem
        self.A_local = self.A[rows, :][:, cols]
        self.b_local = self.b[rows]

    def reload(self):
        """Reload all data from disk and re compute A_local and b_local
        for the same agent_id.
        """
        self._load_and_build_local()

    def __call__(self) -> tuple[np.ndarray, np.ndarray]:
        """Always return the same local (A_local, b_local) for this agent."""
        # Initialize empty arrays if they're None (shouldn't happen normally)
        if self.A_local is None:
            self.A_local = np.array([])
        if self.b_local is None:
            self.b_local = np.array([])

        return self.A_local, self.b_local

    def get_partitions(self) -> tuple[list, list]:
        # Initialize empty arrays if they're None (shouldn't happen normally)
        if self.row_partition is None:
            self.row_partition = []
        if self.col_partition is None:
            self.col_partition = []

        return self.row_partition, self.col_partition
