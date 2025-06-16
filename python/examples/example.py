from skywing.consensus import *
from skywing.agent import Agent
import skywing.skywing_cpp_interface as skywing_cpp_interface
from skywing.algorithms import *
# collective_sum = CppConsensusOp(skywing_cpp_interface.CollectiveDoubleSum)
# consensus_example_max = PythonConsensusOp(ExampleConsensusMaxProcessor)
# consensus_jacobi = PythonConsensusOp(ExampleJacobiLaplacianProcessor)

if __name__ == "__main__":
    import numpy as np

    class SumTask:
        """Simple task that chooses some random values and performs a
        consensus summation. Uses the C++ consensus summation processor."""

        def __init__(self):
            self.value = np.random.randn()
            self.calls = 0

        def __call__(self):
            value = self.value
            sum_result = collective_sum(value)
            print(f"Value: {value}, Sum: {sum_result}")

            self.calls += 1
            return self.calls < 20

    class MaxTask:
        """Simple task that chooses some random values and performs a
        consensus maximum. Uses the example Python consensus maximum
        processor.

        Note: It is not recommended to use the `consensus_example_max`
        in practice as it lacks a Markov consensus. There is a C++
        maximum processor with a Markov consensus that is recommended
        instead.

        """

        def __init__(self):
            self.value = np.random.randn()
            self.calls = 0

        def __call__(self):
            value = self.value
            print("About to get value")
            result = consensus_example_max(value)
            print(f"Value: {value}, Max: {result}")

            self.calls += 1
            if self.calls % 10 == 0:
                self.value = np.random.randn()

            return self.calls < 30

    class JacobiTask:
        """Simple task that chooses some random values for a
        right-hand side and solve the problem `Lx=b` for `x`, where
        `L` is a graph Laplacian with some additional weight on the
        diagonal.

        """

        def __init__(self):
            self.rhs = np.random.randn()
            self.diag = 1.0
            self.off_diag_mag = 1.0
            self.calls = 0

        def __call__(self):
            print("About to get value")
            result = consensus_jacobi(self.diag, self.off_diag_mag, self.rhs)
            print(f"Rhs: {self.rhs}, Result: {result}")

            self.calls += 1
            # if self.calls % 10 == 0:
            #     self.rhs = np.random.randn()

            return self.calls < 60

    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, help="The port this agent will use.")
    parser.add_argument(
        "--nbr_ports",
        type=int,
        nargs="+",
        help="Ports the neighboring agents will use.",
    )
    args = parser.parse_args()

    agent_name = "agent" + str(args.port)
    skywing_agent = Agent(agent_name, "127.0.0.1", args.port)
    if args.nbr_ports is not None:
        nbrs = [("127.0.0.1", p) for p in args.nbr_ports]
        skywing_agent.configure_neighbors(nbrs)

    # Uncomment the test you want to run.
    #    task_fun = JacobiTask()
    task_fun = MaxTask()
    #    task_fun = SumTask()
    skywing_agent.run_continuous(task_fun)
