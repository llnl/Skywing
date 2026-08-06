import numpy as np

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class SGDData(ProcessorData):
    x: np.ndarray


class SGDProcessor(Processor):
    """This implements a simple, distributed, asynchronous stochastic gradient descent
    method for a linear least squares problem. The problem may be either row-partitioned
    or column-partitioned.
    """

    def __init__(self, data: tuple[np.ndarray, np.ndarray], **kwargs):
        """Initialize the SGDProcessor.

        Args:
            data: Tuple of (A, b) where A is the data matrix and b is the target vector
            **kwargs: Keyword arguments, must include 'partitioning' ("row" or "col")
        """
        super().__init__(data, **kwargs)

        # Set default parameters if not given
        if self.parameters.get("partitioning") is None:
            raise RuntimeError(
                'SGDProcessor requires partitioning keyword argument (partitioning="row" or partitioning="col").'
            )
        if self.parameters.get("lam") is None:
            self.parameters["lam"] = 0.0
        if self.parameters.get("K") is None:
            if (
                self.parameters["lam"] != 0.0
                or self.parameters["partitioning"] == "col"
            ):
                raise RuntimeError(
                    "SGDProcessor requires K keyword argument if lam != 0.0 or partitioning == col (K=size of collective)."
                )
            else:
                self.parameters["K"] = 1.0

        # WM: todo - what's a good default learning rate (gamma)?
        #            what about a learning rate schedule?
        if self.parameters.get("gamma") is None:
            self.parameters["gamma"] = 0.01

        # Initialize variables
        A, b = self.data
        partitioning = self.parameters["partitioning"]
        if partitioning == "row":
            self.x = np.zeros(A.shape[1])
            self.result = self.x
        elif partitioning == "col":
            self.x = np.zeros(A.shape[0])
            self.result = A.T @ self.x
        else:
            raise RuntimeError(
                'SGDProcessor requires partitioning to be set to either "row" or "col".'
            )
        col_partition = self.parameters.get("col_partition")
        if col_partition is not None:
            self.result = (self.result, col_partition)

    def process_update(self, my_tag: str, recv_data: dict[str, SGDData]):

        # Get parameters
        gamma = self.parameters["gamma"]

        # Average with neighbors
        N = 0
        self.x = np.zeros(self.x.shape)
        for _tag, nbr_x in recv_data.items():
            # WM: todo - why do I need this guard here? Seeing nbr_x come in as empty the first time?
            if len(nbr_x.x) > 0:
                self.x += nbr_x.x
                N += 1

        if N > 0:
            self.x /= N

        # Update result
        self.update_result()

        # Do a local gradient step
        grad = self.gradient(self.x)
        self.x = self.x - gamma * grad

        self.save_history()

    def prepare_for_publication(self):
        return SGDData(x=self.x)

    def convert(self, publish_data: SGDData):
        return [], list(publish_data.x), []

    def deconvert(self, recv_strings, recv_doubles, recv_ints):
        return SGDData(x=np.array(recv_doubles))

    def gradient(self, x):
        # Problem data and parameters
        A, b = self.data
        partitioning = self.parameters["partitioning"]
        lam = self.parameters["lam"]
        K = self.parameters["K"]

        # Gradient calculation
        if partitioning == "row":
            return 2.0 * A.transpose() @ (A @ x - b) + 2.0 * (lam / K) * x
        elif partitioning == "col":
            return A @ A.transpose() @ x + (lam / K) * x - (1.0 / K) * b

    def update_result(self):
        A, b = self.data
        partitioning = self.parameters["partitioning"]
        col_partition = self.parameters.get("col_partition")
        if partitioning == "row":
            self.result = self.x
        elif partitioning == "col":
            self.result = A.T @ self.x
        if col_partition is not None:
            self.result = (self.result, col_partition)
