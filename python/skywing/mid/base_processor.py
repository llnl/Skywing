import threading
import time
from abc import ABC, abstractmethod
from typing import Any

import numpy as np


# PORT: This is really maintaining compatibility with the C++ linear system driver history files,
#       which may not be necessary at some point.
def convert_numpy_to_history_string(x: np.ndarray, indices=None) -> str:
    string_x = "[ "
    if x.ndim != 1:
        raise ValueError("ERROR: expected 1D array in convert_numpy_to_history_string")
    if indices is None:
        indices = list(range(len(x)))
    for i, index in enumerate(indices):
        string_x += f"({index}, {x[i]}) "
    string_x += "]"
    return string_x


class Processor(ABC):
    """Base class for implementing the local update logic for algorithms.

    Important, general member variables for all inheriting processors include:

    - ``self.data``: The generic data used by the algorithm (may be any type, e.g. a single value, np.array, tuple, dict, etc.)
    - ``self.parameters``: Dict containing any algorithm-specific parameters passed via kwargs.
    - ``self.result``: The algorithm result (may be any type, e.g. a single value, np.array, tuple, dict, etc.)
    - ``self.history_dir``: The directory where iterate history is saved. Also serves as a flag indicating whether iterate history is requested: iterate history is only saved if the "history_dir" kwarg is passed at initialization.

    """

    def __init__(self, data, **kwargs):
        """Initialize the processor with data and parameters.

        Subclasses should call ``super().__init__(data, **kwargs)`` and implement their own
        initialization of ``self.result``, if necessary. Note that ``type(data)`` is
        algorithm dependent and may be a single value, np.array, tuple, dict, etc.
        Similarly, the kwargs are used to pass arbitrary algorithm-specific parameters.
        Note the special kwarg, ``history_dir``, which is used to specify a directory
        for saving the iterate history, if desired.

        Args:
            data: Initial data value saved as ``self.data``
            **kwargs: Keyword arguments saved as ``self.parameters``
        """
        # Input data and parameters
        self.data = data
        self.parameters = kwargs

        # Result should be initialized in subclassed processors if necessary
        self.result = None

        # Threading lock to eliminate potential race conditions between the user,
        # job, and manager threads calling various processor functions, which may
        # attempt to simultaneously update local member variables
        self._lock = threading.Lock()

        # Initialize history file if history_dir kwarg was passed
        self.history_dir = kwargs.get("history_dir")
        if self.history_dir:
            file_id = (
                str(kwargs["agent"].addr)
                + "_"
                + str(kwargs["agent"].port)
                + "_"
                + str(kwargs["unique_id"])
            )
            self.history_file_name = self.history_dir + "/history_" + file_id + ".txt"
            # Create empty history file
            open(self.history_file_name, "w").close()

    def update_data(self, data) -> None:
        """Update ``self.data``. Note that ``type(data)`` is algorithm dependent
        and may be a single value, np.array, tuple, dict, etc.

        Args:
            data: The updated data value
        """
        self.data = data

    def _update_data(self, data) -> None:
        """Calls update_data protected by a threading lock."""
        with self._lock:
            self.update_data(data)

    def update_parameters(self, **kwargs) -> None:
        """Update the arbitrary, algorithm-dependent parameters stored in ``self.parameters``.

        Args:
            **kwargs: Keyword arguments describing parameters to update
        """
        for key, val in kwargs.items():
            self.parameters[key] = val

    def _update_parameters(self, **kwargs) -> None:
        """Calls update_parameters protected by a threading lock."""
        with self._lock:
            self.update_parameters(**kwargs)

    def query(self):
        """Return the current result of the algorithm. Note that the type of
        ``self.result`` is algorithm dependent and implemented in the subclasses.
        """
        return self.result

    def _query(self):
        """Calls query protected by a threading lock."""
        with self._lock:
            return self.query()

    def save_history(self) -> None:
        """Saves the iterate history of the processor in the directory specified
        by the ``history_dir`` kwarg passed as a parameter to the processor.
        Iterate history is only saved if ``history_dir`` is provided.
        """
        if self.history_dir is not None:
            # Get the runtime and cast the current result to string
            runtime = time.monotonic_ns()
            if isinstance(self.result, np.ndarray):
                string_result = convert_numpy_to_history_string(self.result)
            elif (
                isinstance(self.result, tuple)
                and isinstance(self.result[0], np.ndarray)
                and isinstance(self.result[1], list)
            ):
                string_result = convert_numpy_to_history_string(
                    self.result[0], self.result[1]
                )
            else:
                string_result = str(self.result)

            # Append to the history file
            with open(self.history_file_name, "a") as f:
                f.write(str(runtime * 1e-9) + "\t" + string_result + "\n")

    @abstractmethod
    def process_update(self, my_tag: str, recv_data: dict[str, Any]):
        """Subclasses should implement the main local computation step here.

        ``my_tag`` is the local agent's communication tag.
        ``recv_data`` is the data received from communication neighbors
        in the form of a dictionary with tags this agent subscribes to
        as keys. The type of the values (i.e. the communicated data) must
        be defined for each processor as a derived class of ``ProcessorData``.

        Args:
           my_tag: The local agent's communication tag
           recv_data: Dictionary with most recent communicated data from neighbors
        """
        pass

    def _process_update(self, my_tag: str, recv_data: dict[str, Any]):
        """Calls process_update protected by a threading lock."""
        with self._lock:
            self.process_update(my_tag, recv_data)

    @abstractmethod
    def prepare_for_publication(self):
        """Subclasses should implement the appropriate method for returning
        communication data to send to neighbors. The return type here
        must be defined for each processor as a derived class of ``ProcessorData``.
        """
        pass

    def _prepare_for_publication(self):
        """Calls prepare_for_publication protected by a threading lock."""
        with self._lock:
            return self.prepare_for_publication()

    @abstractmethod
    def convert(self, publish_data):
        """Subclasses should implement the appropriate method for converting
        the result of self.prepare_for_publication() to the standard communication
        format: list[str], list[double], list[int].

        self.prepare_for_publication() yields publish_data
        self.convert() converts publish_data => (list[str], list[double], list[int])
        This is used only in the IterationCppCore and CppIteration classes,
        and is ignored with the python core. This wil be deleted once C++ is depreciated.
        """
        pass

    @abstractmethod
    def deconvert(self, recv_strings, recv_doubles, recv_ints):
        """Subclasses should implement the appropriate method for deconverting
        received data in the standard communication format to whatever form
        is needed by the specific, subclassed processor.
        This is used only in the IterationCppCore and CppIteration classes,
        and is ignored with the python core. This wil be deleted once C++ is depreciated.

        self.deconvert() converts (list[str], list[double], list[int]) => recv_data passed to self.process_update()
        """
        # convert list[str], list[double], list[int] into data user
        # wants
        pass
