import numpy as np

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class JacobiData(ProcessorData):
    x_global: np.ndarray
    index: list[int]


class JacobiProcessor(Processor):
    """This implements a simple, distributed, asynchronous Jacobi iteration
    for solving linear systems. The matrix and right-hand side vector
    are assumed to be row-partitioned and distributed and each agent stores
    a local partition of the solution vector.

    In addition to the local matrix and right-hand side, the global indices
    of the local rows must be passed ("row_partition" kwarg).
    """

    def __init__(self, data: tuple[np.ndarray, np.ndarray], **kwargs):
        """Initialize the JacobiProcessor.

        Args:
            data: Tuple of (A, b) where A is the local matrix partition and b is the local RHS vector
            **kwargs: Keyword arguments, must include 'row_partition' (global indices of local rows)
        """
        super().__init__(data, **kwargs)

        # Initialize x_global, a local representation of the global solution
        # vector that will store local updates as well as communicated updates
        # from neighbors. Note that if update_data() is called to get new
        # matrix/rhs data, x_global is NOT reset, which will have the effect of
        # retaining the current iterate as an initial guess for the updated problem.
        A, b = self.data
        self.x_global = np.zeros(A.shape[1])

        # Initialize result (note that the result here is actually a tuple
        # including the local x and the associated global indices)
        row_partition = self.parameters["row_partition"]
        self.result = (self.x_global[row_partition], row_partition)

    def process_update(self, my_tag: str, recv_data: dict[str, JacobiData]):

        # Problem data and parameters
        A, b = self.data
        row_partition = self.parameters["row_partition"]
        D_inv = np.diag(1.0 / A[np.arange(len(row_partition)), row_partition])
        omega = self.parameters.get("omega", 1.0)

        # Get updates from neighbors
        for _tag, nbr_data in recv_data.items():
            self.x_global[nbr_data.index] = nbr_data.x_global

        # Do a local Jacobi update
        self.x_global[row_partition] += omega * D_inv @ (b - A @ self.x_global)

        # Update local result
        self.result = (self.x_global[row_partition], row_partition)

        self.save_history()

    def prepare_for_publication(self):
        x, i = self.result
        return JacobiData(x_global=x, index=i)

    def convert(self, publish_data: JacobiData):
        return [], list(publish_data.x_global), publish_data.index

    def deconvert(self, recv_strings, recv_doubles, recv_ints):
        return JacobiData(x_global=np.array(recv_doubles), index=recv_ints)
