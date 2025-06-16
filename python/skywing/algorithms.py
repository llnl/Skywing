from skywing.consensus import CppConsensusOp, PythonConsensusOp
from skywing.skywing_cpp_interface import CollectiveDoubleSum

from skywing.consensus import (
    ExampleConsensusMaxProcessor,
    ExampleJacobiLaplacianProcessor,
)


collective_sum = CppConsensusOp(CollectiveDoubleSum)
consensus_example_max = PythonConsensusOp(ExampleConsensusMaxProcessor)
consensus_jacobi = PythonConsensusOp(ExampleJacobiLaplacianProcessor)
