import numpy as np

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class ADMMData(ProcessorData):
    q_j: dict[str, np.ndarray]
    my_iter: int


class ADMMProcessor(Processor):
    """Alternating direction method of multipliers (ADMM) for a linear least
    squares problem.

    We implement the robust and asynchronous distributed relaxed ADMM (R-ADMM) method from the following paper:

    Asynchronous Distributed Optimization Over Lossy Networks via Relaxed ADMM: Stability and Linear Convergence
    Nicola Bastianello, Ruggero Carli, Luca Schenato, and Marco Todescato
    IEEE Transactions on Automatic Control, 2021
    https://arxiv.org/pdf/1901.09252

    The method is applied to a l2-regularized linear least squares problem.

    min_x ||Ax - b||^2_2 - lambda ||x||^2_2

    WM: todo - it would be nice to also implement the row-partitioned case here

    In the column-partitioned case, the problem needs to be transformed in order for the objective to be
    separable among the distributed agents. We use the strategy described in the following paper:

    Distributed Ridge Regression with Feature Partitioning
    Cristiano Gratton, Naveen K. D. Venkategowda, Reza Arablouei, Stefan Werner
    In proceedings: 2018 52nd Asilomar Conference on Signals, Systems, and Computers
    https://ieeexplore.ieee.org/document/8645549

    Note that the solution to the minimization problem can be
    obtained via the normal equations as:

    x = A^T (A A^T + lambda I)^-1 b

    Let M = (A A^T + lambda I). Note that solving for v = M^-1 b is equivalent to solving the minimization:

    min_v ((1/2)v^T M v - v^T b)

    This minimization now has an objective that is separable across agents. Letting M_k = ( A_k A_k^T + (lambda / K) I ), then

    M = sum_k ( A_k A_k^T + (lambda / K) I )
    f(v) = (1/2)v^T M v - v^T b = sum_k ( (1/2)v_k^T M_k v_k - (1/K) v_k^T b ) = sum_k f_k(v_k)

    We can now apply R-ADMM to the separable objective above.
    """

    def __init__(self, data: tuple[np.ndarray, np.ndarray], **kwargs):
        """The data passed to the processor should be the local columns of the matrix, A,
        and the right-hand side vector, b.
        The parameters passed as kwargs include:
        K     = (required) the total number of agents
        N_k   = (required) the number of neighbors for this agent
        lam   = a scalar determinining the size of the Tikhonov regularization term
        rho   = a scalar determinining the penalty parameter
        alpha = a scalar determinining the relaxation parameter
        """
        super().__init__(data, **kwargs)

        # Initialize the result (solution stored as x with optional indices from
        # col_partition kwarg) and v (the local consensus variable approximating Ax)
        A, b = self.data
        col_partition = self.parameters.get("col_partition")
        self.x = np.zeros(A.shape[1])
        if col_partition is not None:
            self.result = (self.x, col_partition)
        else:
            self.result = self.x
        self.v = np.zeros(A.shape[0])
        self.my_iter = 0
        self.my_tag = ""

        # Check for required parameters and initialize parameters not given by user
        if self.parameters.get("K") is None:
            raise RuntimeError(
                "ADMMProcessor requires K keyword argument (K=size of collective)."
            )
        if self.parameters.get("N_k") is None:
            raise RuntimeError(
                "ADMMProcessor requires N_k keyword argument (N_k=number of communication neighbors)."
            )
        # WM: todo - placeholder below for when I implement the row-partitioned version
        # if self.parameters.get("partitioning") is None:
        #     raise RuntimeError("ADMMProcessor requires partitioning keyword argument (partition=\"row\" or partition=\"col\").")
        # WM: todo - there are strategies for picking good values of rho and alpha... look
        #            into these and implement something. For now, simple defaults are fine.
        if self.parameters.get("rho") is None:
            self.parameters["rho"] = 1.0
        if self.parameters.get("lam") is None:
            self.parameters["lam"] = 0.0
        if self.parameters.get("alpha") is None:
            self.parameters["alpha"] = 0.9

        # Initialize auxiliary consensus variables and iteration counters for each neighbor
        self.z_j = {}
        self.iter_j = {}

        # Setup the QR decomposition to be used during process_update()
        self.setup_qr()

    def update_data(self, data):
        """When updating data, need to recompute the QR decomposition."""
        super().update_data(data)
        A, b = self.data
        self.v = np.zeros(A.shape[0])
        # WM: todo - if iteration is ongoing while update_data() is called,
        # resetting the following dicts can cause key errors! Need to fix!
        self.z_j = {}
        self.iter_j = {}
        self.setup_qr()

    def process_update(self, my_tag: str, recv_data: dict[str, ADMMData]):

        # Problem data and parameters
        A, b = self.data
        K = self.parameters["K"]
        alpha = self.parameters["alpha"]
        col_partition = self.parameters.get("col_partition")

        # WM: todo - can I get my_tag at initialization?
        self.my_tag = my_tag

        # Update z_j <- (1 - alpha) * z_j + alpha * q_j
        for _tag, nbr_data in recv_data.items():
            # Build up the z_j dict with incoming tags
            if _tag not in self.z_j:
                self.z_j[_tag] = np.zeros(self.v.shape)
                self.iter_j[_tag] = 0
            # Parse data sent by nbr
            q_j = nbr_data.q_j.get(self.my_tag)
            if q_j is not None:
                # Update z_j for this nbr
                if len(q_j) > 0:
                    # WM: note - I'm using an iteration counter to see if the data has been updated
                    #            is there a cleaner/better way to get this info from skywing core?
                    if nbr_data.my_iter > self.iter_j[_tag]:
                        self.z_j[_tag] = (1.0 - alpha) * self.z_j[_tag] + alpha * q_j
                        self.iter_j[_tag] = nbr_data.my_iter

        # Solve the local subproblem,
        # v <- M^{-1} [ ( 1 / K ) * b + sum_z_j ]
        # via the QR decomposition computed in setup_qr()
        sum_z_j = np.zeros(self.v.shape)
        for zj in self.z_j.values():
            sum_z_j += zj
        rhs = (1.0 / K) * b + sum_z_j
        self.v = np.linalg.solve(self.R, self.Q.T @ rhs)

        # Update x <- A^T v
        self.x = A.T @ self.v
        if col_partition is not None:
            self.result = (self.x, col_partition)
        else:
            self.result = self.x

        self.my_iter += 1
        self.save_history()

    def prepare_for_publication(self):
        """Send q_j = -z_j + 2 * rho * v

        WM: todo - different info is exchanged between different pairs of neighbors.
        So I'm not just publishing a single value for everyone else to read here.
        Theoretically, I guess you could do this by setting up a bunch of different
        publications and having different neighbors subscribe to the appropriate one?
        But this doesn't fit the mold for the current iterative method...
        My solution here is to just send everything and have nbrs parse out what they need.
        A little cumbersome, and we are transmitting more data than needed... but should work.
        """
        # WM: todo - prepare_for_publication() will be called for the first time before process_update(),
        #            which is where self.z_j gets populated. So I'm sending trivial info on the first
        #            iteration. This is fine, I guess (works), but is a bit weird. Is there a better way?
        rho = self.parameters["rho"]
        q_j = {_tag: -self.z_j[_tag] + 2.0 * rho * self.v for _tag in self.z_j}

        # WM: todo - for now, send type is (dict, int)
        return ADMMData(q_j=q_j, my_iter=self.my_iter)

    def setup_qr(self):
        """Helper function to setup the matrix, M = [AA^T + (lam / K + 2 * rho * N_k) * I]
        and compute its QR decomposition.
        """
        # Get the matrix, A, and right-hand side, b
        A, b = self.data

        # Get the regularization parameter, lam (unregularized by default)
        K = self.parameters["K"]
        N_k = self.parameters["N_k"]
        lam = self.parameters["lam"]
        rho = self.parameters["rho"]

        # Construct the matrix, M
        M = A @ A.T + (lam / K + rho * N_k) * np.eye(A.shape[0])

        # Compute the QR decomposition of M
        self.Q, self.R = np.linalg.qr(M)
