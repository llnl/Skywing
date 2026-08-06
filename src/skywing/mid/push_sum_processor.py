from copy import copy
from typing import Union

import numpy as np

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class PushSumData(ProcessorData):
    sigma_x: Union[float, np.ndarray]
    sigma_y: Union[float, np.ndarray]


class PushSumProcessor(Processor):
    """This implements the perturbed, asynchronous push sum algorithm
    (P-ASY-SUM-PUSH) presented as algorithm 1 in the following paper:

    Achieving Linear Convergence in Distributed Asynchronous Multiagent Optimization
    Ye Tian, Ying Sun, and Gesualdo Scutari
    IEEE Transactions on Automatic Control
    https://arxiv.org/pdf/1803.10359

    This variant of push sum is robust to asynchrony and packet loss,
    communicates information with an arbitrary number of neighbors
    on a connected di-graph, and allows for a perturbation signal that
    extends the algorithm to track the average of a changing signal.
    """

    def __init__(self, data, **kwargs):
        """Initialize the PushSumProcessor.

        Args:
            data: Initial data value
            **kwargs: Keyword arguments, must include 'N_k' (neighborhood size)
        """
        super().__init__(data, **kwargs)

        # Check required parameters
        if self.parameters.get("N_k") is None:
            raise RuntimeError(
                "PushSumProcessor requires N_k keyword argument (N_k=size of neighborhood)."
            )

        # Get the starting data and parameters
        starting_value = self.data

        # Initialize variables
        self.x = starting_value - starting_value
        self.y = 1.0
        self.epsilon = copy(starting_value)
        self.sigma_x = starting_value - starting_value
        self.sigma_y = 0.0
        self.nbr_x = {}
        self.nbr_y = {}
        self.nbr_x_prev = {}
        self.nbr_y_prev = {}
        self.result = self.x / self.y

    def update_data(self, data):
        self.epsilon = self.data
        super().update_data(data)
        self.epsilon = self.data - self.epsilon

    def process_update(self, my_tag: str, recv_data: dict[str, PushSumData]):

        # WM: todo - get N_k automatically?
        N_k = self.parameters["N_k"]
        w = 1.0 / N_k

        # Collect neighbor data
        for _tag, nbr_data in recv_data.items():
            if _tag != my_tag:
                self.nbr_x[_tag] = nbr_data.sigma_x
                self.nbr_y[_tag] = nbr_data.sigma_y

        x_diff = sum(self.nbr_x.values()) - sum(self.nbr_x_prev.values())
        y_diff = sum(self.nbr_y.values()) - sum(self.nbr_y_prev.values())

        # If data was updated, add epsilon
        if self.epsilon is not None:
            x_diff += self.epsilon
            self.epsilon = None

        # Update local variables
        self.x += x_diff
        self.y += y_diff

        x_diff_bool = abs(x_diff) > 0.0
        if isinstance(x_diff, np.ndarray):
            x_diff_bool = x_diff_bool.any()
        if x_diff_bool or abs(y_diff) > 0.0:
            self.sigma_x = self.sigma_x + w * self.x
            self.sigma_y = self.sigma_y + w * self.y

            self.x = w * self.x
            self.y = w * self.y

            for _tag in self.nbr_x:
                self.nbr_x_prev[_tag] = copy(self.nbr_x[_tag])
                self.nbr_y_prev[_tag] = copy(self.nbr_y[_tag])

            # Update result
            self.result = self.x / self.y

    def prepare_for_publication(self):
        return PushSumData(sigma_x=self.sigma_x, sigma_y=self.sigma_y)
