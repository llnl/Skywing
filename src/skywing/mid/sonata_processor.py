import numpy as np

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class SONATAData(ProcessorData):
    rho: np.ndarray
    v: np.ndarray


class SONATAProcessor(Processor):
    """This implements the asynchronous variant of SONATA described in the paper:

    Achieving Linear Convergence in Distributed Asynchronous Multiagent Optimization
    Ye Tian, Ying Sun, and Gesualdo Scutari
    IEEE Transactions on Automatic Control
    https://arxiv.org/pdf/1803.10359

    This is a distributed gradient descent algorithm with gradient tracking. The
    gradient tracking is implemented via a push sum in order to make it robust
    to asynchrony.
    """

    def __init__(self, data: tuple[np.ndarray, np.ndarray], **kwargs):
        """Initialize the SONATAProcessor.

        Args:
            data: Tuple of (A, b) where A is the data matrix and b is the target vector
            **kwargs: Keyword arguments, must include 'K', 'N_k', and 'partitioning'
        """
        super().__init__(data, **kwargs)

        # Set default parameters if not given
        if self.parameters.get("K") is None:
            raise RuntimeError(
                "SONATAProcessor requires K keyword argument (K=size of collective)."
            )
        if self.parameters.get("N_k") is None:
            raise RuntimeError(
                "SONATAProcessor requires N_k keyword argument (N_k=number of communication neighbors)."
            )
        if self.parameters.get("partitioning") is None:
            raise RuntimeError(
                'SONATAProcessor requires partitioning keyword argument (partition="row" or partition="col").'
            )
        if self.parameters.get("lam") is None:
            self.parameters["lam"] = 0.0
        if self.parameters.get("gamma") is None:
            self.parameters["gamma"] = 0.01

        # Initialize some variables
        A, b = self.data
        partitioning = self.parameters["partitioning"]
        if partitioning == "row":
            size = A.shape[1]
        elif partitioning == "col":
            size = A.shape[0]
        else:
            raise RuntimeError(
                'SGDProcessor requires partitioning to be set to either "row" or "col".'
            )
        self.x = np.zeros(size)
        self.v = np.zeros(size)
        self.rho = np.zeros(size)
        self.v_j = {}
        self.rho_j = {}
        self.rho_j_prev = {}
        col_partition = self.parameters.get("col_partition")
        self.result = np.zeros(A.shape[1])
        if col_partition is not None:
            self.result = (self.x, col_partition)
        self.my_tag = ""

        # Initialize z as the initial gradient
        self.z = self.gradient(self.x)

    def update_data(self, data):
        super().update_data(data)
        A, b = self.data
        partitioning = self.parameters["partitioning"]
        if partitioning == "row":
            size = A.shape[1]
        elif partitioning == "col":
            size = A.shape[0]
        else:
            raise RuntimeError(
                'SGDProcessor requires partitioning to be set to either "row" or "col".'
            )
        self.x = np.zeros(size)
        self.v = np.zeros(size)
        self.rho = np.zeros(size)
        self.v_j = {}
        self.rho_j = {}
        self.rho_j_prev = {}
        col_partition = self.parameters.get("col_partition")
        if col_partition is not None:
            self.result = (self.x, col_partition)
        else:
            self.result = self.x

        # Initialize z as the initial gradient
        K = self.parameters["K"]
        lam = self.parameters["lam"]
        self.z = 2.0 * A.transpose() @ (A @ self.x - b) + 2.0 * (lam / K) * self.x

    def process_update(self, my_tag: str, recv_data: dict[str, SONATAData]):

        # Get parameters
        K = self.parameters["K"]
        N_k = self.parameters["N_k"]
        gamma = self.parameters["gamma"]

        # WM: note - using uniform weight and assuming bi-directional communication
        #     todo - is there a nicer way to get the number of neighbors without passing N_k?
        w = 1.0 / N_k

        # WM: todo - can I get my_tag at initialization?
        self.my_tag = my_tag

        # Gather info from neighbors
        for _tag, nbr_data in recv_data.items():
            # WM: note that I need to exclude my tag here
            if len(nbr_data.v) and _tag != my_tag:
                self.rho_j[_tag] = nbr_data.rho
                self.v_j[_tag] = nbr_data.v

        # Local descent
        # WM: todo - the paper has suggestions for a schedule for the step size
        self.v = self.x - gamma * K * self.z

        # Get old gradient
        grad_old = self.gradient(self.x)

        # Consensus (x tracks average of v_j's)
        self.x = w * self.v + w * sum(self.v_j.values())

        # Get new gradient
        grad_new = self.gradient(self.x)

        # Gradient tracking sum step
        self.z = (
            self.z
            + sum(self.rho_j.values())
            - sum(self.rho_j_prev.values())
            + grad_new
            - grad_old
        )

        # Gradient tracking mass-buffer update (keep prev values of rho)
        for _tag in self.rho_j:
            self.rho_j_prev[_tag] = self.rho_j[_tag].copy()

        # Gradient tracking push step
        self.rho = self.rho + w * self.z
        self.z = w * self.z

        # Update result
        self.update_result()

        self.save_history()

    def prepare_for_publication(self):
        return SONATAData(rho=self.rho, v=self.v)

    def gradient(self, x):

        # Problem data and parameters
        A, b = self.data
        K = self.parameters["K"]
        lam = self.parameters["lam"]
        partitioning = self.parameters["partitioning"]

        # Gradient calculation
        if partitioning == "row":
            return 2.0 * A.transpose() @ (A @ x - b) + 2.0 * (lam / K) * x
        elif partitioning == "col":
            return A @ A.transpose() @ x + (lam / K) * x - (1.0 / K) * b
        else:
            raise RuntimeError(
                'SONATAProcessor: set partitioning keyword argument to "row" or "col".'
            )

    def update_result(self):
        A, b = self.data
        partitioning = self.parameters["partitioning"]
        col_partition = self.parameters.get("col_partition")

        if partitioning == "row":
            self.result = self.x
        elif partitioning == "col":
            self.result = A.T @ self.x
        else:
            raise RuntimeError(
                'SONATAProcessor: set partitioning keyword argument to "row" or "col".'
            )
        if col_partition is not None:
            self.result = (self.result, col_partition)
