"""Test suite to validate example scripts using predefined input data and agent configurations.

This module defines an abstract ExampleTest base class describing the interface for
individual example tests, and concrete implementations for Max, Sum, and SGD examples.

Each example defines:
- Paths to the example script, input data, and config file
- How to compute the expected output
- How to validate the output produced by agent processes

The tests launch the agents, wait for outputs, and assert correctness against expected values.
"""

import os
import time
from abc import ABC, abstractmethod
from datetime import datetime
from pathlib import Path

import numpy as np
import pytest
from skywing.drivers.driver import load_collective_config, run_collective
from skywing.drivers.utils.data_loaders import CSVDataLoader
from skywing.drivers.utils.matrix_gen import read_partition

# This finds the path to the Skywing/python directory.
# This is used to find the test config files, and to save the artifacts
ROOT_DIR = Path(__file__).resolve().parents[1]


def find_latest_run_dir(example_name):
    # If a test run did not produce an output and silently fails
    # this portion may still find a previous successful run. It must fail loudly earlier if it does not produce an output.
    """Find the most recent run directory for a given example name."""
    artifact_dir = os.path.join("artifacts", example_name)
    if not os.path.exists(artifact_dir):
        return None
    run_dirs = [d for d in os.listdir(artifact_dir) if d.startswith("run_")]
    if not run_dirs:
        return None
    run_dirs.sort(reverse=True)
    return os.path.join(artifact_dir, run_dirs[0])


def compare_np_output_to_tol(content: np.ndarray, expected: np.ndarray, tol: float):
    """Helper function for comparing difference between numpy arrays
    of arbitrary shape to a given tolerance in the infinity norm.
    The comparison is row-wise, that is, we compare:
        content[i, ...] - expected[i, ...] for all i
    """
    # Loop over rows (axis 0) and take the norm of the difference
    for i in range(content.shape[0]):
        diff = content[i, ...] - expected[i, ...]
        diff_norm = np.linalg.norm(np.atleast_1d(diff), ord=np.inf)
        assert diff_norm < tol


def solve_linear_system(data_file: str, agent_id: int) -> np.ndarray:
    """Helper function for reading in and solving a linear system
    using numpy's linalg.lstsq() routine. The returned solution is
    constrained to the indices owned by the specified agent.
    """
    A = np.loadtxt(f"{data_file}/A.txt")
    b = np.loadtxt(f"{data_file}/b.txt")
    col_partition = read_partition(f"{data_file}/col_partition.txt")
    x_star, residuals, rank, s = np.linalg.lstsq(A, b, rcond=None)
    return x_star[col_partition[agent_id]]


class ExampleTest(ABC):
    """Abstract base class defining the interface for example tests.

    Each example test must define:
    - example_path: path to the example script
    - data_file: path to example's input data
    - config_file: path to config JSON for agents
    - expected_output(agent_id): compute expected output value for the given agent
    - assert_output(content, expected): assertion for output correctness
    """

    def __init__(
        self,
        example_path,
        config_file,
        data_file="",
        name=None,
        backend="python",
        **kwargs,
    ):
        # By default, expect the paths to be relative to Skywing/python
        self.example_path = str(ROOT_DIR / example_path)
        self.data_file = str(ROOT_DIR / data_file) if data_file else ""
        self.config_file = str(ROOT_DIR / config_file)
        # Infer example name from the file name if not provided
        self.name = name or os.path.splitext(os.path.basename(example_path))[0]
        # Backend to use for the iteration (python or cpp)
        self.backend = backend
        # Additional test-specific kwargs
        self.kwargs = kwargs

    @abstractmethod
    def expected_output(self, agent_id):
        """Return the expected value; to be overridden by subclasses."""
        raise NotImplementedError

    @abstractmethod
    def assert_output(self, content, expected) -> None:
        """Assert if content matches expected output; to be overridden by subclasses."""
        raise NotImplementedError


# ---------- concrete example test implementations ----------


class CountExample(ExampleTest):
    def expected_output(self, agent_id: int) -> int:
        collective_config = load_collective_config(self.config_file)
        num_agents = len(collective_config)
        return num_agents

    def assert_output(self, content: np.ndarray, expected: int) -> None:
        assert int(content[0]) == expected


class MaxExample(ExampleTest):
    def expected_output(self, agent_id: int) -> np.ndarray:
        global_data = CSVDataLoader(self.data_file, agent_id).get_global_data()
        return global_data.max(axis=1)

    def assert_output(self, content: np.ndarray, expected: np.ndarray) -> None:
        compare_np_output_to_tol(content, expected, 0.00001)


class PushSumExample(ExampleTest):
    def expected_output(self, agent_id):
        global_data = CSVDataLoader(self.data_file, agent_id).get_global_data()
        return global_data.mean(axis=1)

    def assert_output(self, content: np.ndarray, expected) -> None:
        compare_np_output_to_tol(content, expected, 0.00001)


class SumExample(ExampleTest):
    def expected_output(self, agent_id):
        global_data = CSVDataLoader(self.data_file, agent_id).get_global_data()
        return global_data.sum(axis=1)

    def assert_output(self, content: np.ndarray, expected) -> None:
        compare_np_output_to_tol(content, expected, 0.00001)


class SumCppExample(ExampleTest):
    def expected_output(self, agent_id):
        global_data = CSVDataLoader(self.data_file, agent_id).get_global_data()
        return global_data.sum(axis=1)

    def assert_output(self, content: np.ndarray, expected) -> None:
        compare_np_output_to_tol(content, expected, 0.0001)


class MultiIterExample(ExampleTest):
    def expected_output(self, agent_id):
        global_data = CSVDataLoader(self.data_file, agent_id).get_global_data()
        row_max = global_data.max(axis=1)
        count_less_than_max = np.array(
            [(global_data[i] < row_max[i]).sum() for i in range(global_data.shape[0])]
        )
        return count_less_than_max

    def assert_output(self, content: np.ndarray, expected) -> None:
        compare_np_output_to_tol(content, expected, 0.001)


class SGDExample(ExampleTest):
    def expected_output(self, agent_id):
        return np.array([solve_linear_system(self.data_file, agent_id)])

    def assert_output(self, content: np.ndarray, expected):
        compare_np_output_to_tol(content, expected, 0.001)


class ADMMExample(ExampleTest):
    def expected_output(self, agent_id):
        return np.array([solve_linear_system(self.data_file, agent_id)])

    def assert_output(self, content: np.ndarray, expected):
        compare_np_output_to_tol(content, expected, 0.002)


class PyJobExample(ExampleTest):
    """
    A test class that only checks if an example runs without errors.
    It doesn't verify the content of the output file.
    """

    def expected_output(self, agent_id):
        # Return any value since we won't be checking it
        return 0

    def assert_output(self, content: np.ndarray, expected) -> None:
        # Just check that the output file exists and has content
        assert content is not None and len(content) > 0


# WM: todo - finish out the tests below
# class SimpleSumExample(ExampleTest):
#     def expected_output(self, agent_id):
#         df = pd.read_csv(self.data_file, header=None)
#         return df.iloc[0].sum()

#     def assert_output(self, content: str, expected) -> None:
#         # Assert values match within a small tolerance
#         assert expected == pytest.approx(float(content), abs=0.00001)


# All paths are relative to Skywing/python
# Examples that support both backends - will be tested with both python and cpp
EXAMPLES_BOTH = [
    # WM: todo - automatically expand example to do scalar/vector/matrix
    #            similar to what's done for backends? Maybe wait to do this
    #            until we get rid of cpp backend to avoid the complexity
    #            of nested expansions...
    MaxExample(
        name="max_scalar",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/scalar_test.csv",
        config_file="tests/test_configs/3_line.json",
        processor="Max",
        data_type="scalar",
    ),
    PushSumExample(
        name="push_sum",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/scalar_test.csv",
        config_file="tests/test_configs/3_line.json",
        processor="PushSum",
    ),
    ADMMExample(
        name="admm",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/random_matrix_col_partitioned_3/",
        config_file="tests/test_configs/3_line.json",
        data_type="linear_system",
        num_data_updates=0,
        update_freq=5.0,
        processor="ADMM",
    ),
]

# Examples that only use C++ backend
EXAMPLES_CPP = [
    SumCppExample(
        example_path="examples/sum_cpp.py",
        data_file="tests/test_data/scalar_test.csv",
        config_file="tests/test_configs/3_line.json",
        backend="cpp",
    ),
]

# Examples that only use Python backend
EXAMPLES_PYTHON = [
    CountExample(
        name="count",
        example_path="examples/simple_iteration.py",
        config_file="tests/test_configs/10_ring.json",
        backend="python",
        num_data_updates=0,
        processor="Count",
    ),
    MaxExample(
        name="max_vector",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/vector_test.csv",
        config_file="tests/test_configs/3_line.json",
        processor="Max",
        data_type="vector",
    ),
    MaxExample(
        name="max_matrix",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/matrix_test.csv",
        config_file="tests/test_configs/3_line.json",
        processor="Max",
        data_type="matrix",
    ),
    MultiIterExample(
        example_path="examples/2iters.py",
        data_file="tests/test_data/scalar_test.csv",
        config_file="tests/test_configs/3_line.json",
        backend="python",
    ),
    # Example that just checks if the script runs without errors
    PyJobExample(
        example_path="examples/pyjob.py",
        data_file="tests/test_data/scalar_test.csv",
        config_file="tests/test_configs/3_line.json",
        backend="python",
    ),
    # WM: todo - finish out the tests below
    # SimpleSumExample(
    #     name="simple_sum",
    #     example_path="examples/simple_iteration.py",
    #     data_file="tests/test_data/scalar_test_10.csv",
    #     config_file="tests/test_configs/10_ring.json",
    #     backend="python",
    #     num_calls=1,
    #     processor="SimpleSum",
    # ),
    SGDExample(
        name="sgd_row",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/random_matrix_row_partitioned_3/",
        config_file="tests/test_configs/3_full.json",
        backend="python",
        data_type="linear_system",
        partitioning="row",
        gamma=0.1,
        num_data_updates=0,
        update_freq=5.0,
        processor="SGD",
    ),
    SGDExample(
        name="sgd_col",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/random_matrix_col_partitioned_3/",
        config_file="tests/test_configs/3_full.json",
        backend="python",
        data_type="linear_system",
        partitioning="col",
        gamma=0.1,
        num_data_updates=0,
        update_freq=5.0,
        processor="SGD",
    ),
    SumExample(
        name="sum",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/scalar_test.csv",
        config_file="tests/test_configs/3_line.json",
        backend="python",
        data_type="scalar",
        processor="Sum",
    ),
]


def _expand_examples_with_backends(examples_list, backends):
    """Create test variants for examples that support multiple backends.

    Returns a list of tuples (example_instance, backend_name) to preserve
    backend info for test ID generation.
    """
    expanded = []
    for example in examples_list:
        for backend in backends:
            # Create a copy with the specified backend
            example_copy = type(example)(
                name=example.name,
                example_path=example.example_path,
                data_file=example.data_file,
                config_file=example.config_file,
                backend=backend,
                **example.kwargs,
            )
            expanded.append((example_copy, backend))
    return expanded


# Combine all examples, expanding EXAMPLES_BOTH to test both backends
_EXAMPLES_BOTH_EXPANDED = _expand_examples_with_backends(
    EXAMPLES_BOTH, ["python", "cpp"]
)
EXAMPLES = [ex for ex, _ in _EXAMPLES_BOTH_EXPANDED] + EXAMPLES_CPP + EXAMPLES_PYTHON


def _generate_test_id(example):
    """Generate test ID, adding backend suffix for dual-backend examples."""
    # Check if this example came from EXAMPLES_BOTH
    for ex, backend in _EXAMPLES_BOTH_EXPANDED:
        if ex is example:
            return f"{example.name}_{backend}"
    # Otherwise use the example name as-is
    return example.name


@pytest.mark.parametrize("example", EXAMPLES, ids=_generate_test_id)
def test_example_outputs(example: ExampleTest):
    """Run the agents for each example and verify their outputs."""

    # Use the example name with date time for the output directory
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    output_dir = os.path.join("artifacts", example.name, f"run_{timestamp}")
    os.makedirs(output_dir, exist_ok=True)

    # Run collective for this example
    run_collective(
        str(example.example_path),
        config=str(example.config_file),
        unbuffered=True,
        backend=example.backend,
        output_dir=output_dir,
        data_file=example.data_file,
        **example.kwargs,
    )

    # WM: todo - is run_dir not just output_dir?
    run_dir = find_latest_run_dir(example.name)
    assert run_dir is not None, f"No run directory found for {example.name}"

    time.sleep(1)  # Ensure file writes are complete before reading output

    collective_config = load_collective_config(example.config_file)
    for agent_id in collective_config.keys():
        out_path = os.path.join(run_dir, f"output_{agent_id}.npy")
        assert os.path.exists(out_path), f"Missing output file: {out_path}"
        content = np.load(out_path)
        expected_output = example.expected_output(agent_id)
        example.assert_output(content, expected_output)
