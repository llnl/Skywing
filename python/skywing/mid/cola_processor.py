import numpy as np

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class COLAData(ProcessorData):
    v: np.ndarray


class COLAProcessor(Processor):
    """Communication-Efficient Decentralized Linear Learning (COLA) for
    linear least squares

    The COLA algorithm is a decentralize optimization algorithm that solves
    the optimization problem:
    min_x ( f(Ax) + sum_i (g_i(x_i)) )
    where x in R^n and A is (d x n) data matrix whose columns are distributed
    among distributed processes. See the following for details on the algorithm:
    https://arxiv.org/abs/1808.04883
    This processor implements COLA for a linear least squares objective with
    Tikhonov regularization:
    min_x ( ||Ax - b||^2_2 + lambda * ||x||^2 )
    """

    def __init__(self, data: tuple[np.ndarray, np.ndarray], **kwargs):
        """The data passed to the processor should be the local columns of the matrix, A,
        and the right-hand side vector, b.
        The parameters include:
        K     = (required) the total number of agents
        lam   = a scalar determinining the size of the Tikhonov regularization term
        gamma = a scalar determinining the step size of the update (usually 1.0)
        """
        super().__init__(data, **kwargs)

        # Initialize the result (solution stored as x with optional indices from
        # col_partition kwarg) and v (the local consensus variable approximating Ax)
        A, b = self.data
        col_partition = self.parameters.get("col_partition", None)
        self.x = np.zeros(A.shape[1])
        if col_partition is not None:
            self.result = (self.x, col_partition)
        else:
            self.result = self.x
        self.v = np.zeros(A.shape[0])

        # Setup the QR decomposition to be used during process_update()
        self.setup_qr()

    def update_data(self, data):
        """When updating data, need to recompute the QR decomposition."""
        super().update_data(data)
        A, b = self.data
        self.v = np.zeros(A.shape[0])
        self.x = np.zeros(A.shape[1])
        self.setup_qr()

    def process_update(self, my_tag: str, recv_data: dict[str, COLAData]):

        # Problem data and parameters
        A, b = self.data
        K = self.parameters["K"]  # Note that the collective size, K, is required
        gamma = self.parameters.get("gamma", 1.0)
        lam = self.parameters.get("lam", 0.0)
        col_partition = self.parameters.get("col_partition", None)

        # Update v as average over neighbors: v <- /sum_l (v_l) / N
        N = 0
        self.v = np.zeros(A.shape[0])
        for _tag, nbr_v in recv_data.items():
            # WM: todo - why do I need this guard here?
            if len(nbr_v.v) > 0:
                self.v += nbr_v.v
                N += 1
        if N > 0:
            self.v /= N

        # Solve the local subproblem,
        # [A ; sqrt(lam / K) * I] * delta_x = [(b - v) / K ; -sqrt(lam / K) * x]
        # via the QR decomposition computed in setup_qr()
        if lam > 0.0:
            rhs = np.hstack(((b - self.v) / K, -np.sqrt(lam / K) * self.x))
        else:
            rhs = (b - self.v) / K
        delta_x = np.linalg.solve(self.R, self.Q.T @ rhs)

        # Update x <- x + gamma * delta_x
        self.x += gamma * delta_x
        if col_partition is not None:
            self.result = (self.x, col_partition)
        else:
            self.result = self.x

        # Update v <- v + gamma * K * A * delta_x
        self.v += gamma * K * A @ delta_x

        self.save_history()

    def prepare_for_publication(self):
        return COLAData(v=self.v)

    def convert(self, publish_data: COLAData):
        return [], list(publish_data.v), []

    def deconvert(self, recv_strings, recv_doubles, recv_ints):
        return COLAData(v=np.array(recv_doubles))

    def setup_qr(self):
        """Helper function to setup the matrix, M = [A ; sqrt(lam / K) * I]
        and compute its QR decomposition.
        """
        # Get the matrix, A, and right-hand side, b
        A, b = self.data

        # Get the regularization parameter, lam (unregularized by default)
        lam = self.parameters.get("lam", 0.0)
        K = self.parameters["K"]

        # Construct the matrix M = [A ; sqrt(lam / K) * I] representing the regularized problem
        if lam > 0.0:
            M = np.vstack((A, np.sqrt(lam / K) * np.eye(A.shape[1])))
        else:
            M = A

        # Compute the QR decomposition of M
        self.Q, self.R = np.linalg.qr(M)
