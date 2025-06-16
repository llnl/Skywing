import functools
import inspect
from typing import Dict, List, Tuple, Any
from abc import ABC, abstractmethod

from skywing.agent import Agent
import skywing.skywing_cpp_interface as skywing_cpp_interface

"""
This module provides a framework for implementing and running distributed consensus algorithms
in a network of agents. It defines base classes and utilities for creating both Python-based
and C++-based consensus operations, as well as specific consensus processors for algorithms
like maximum value and Jacobi Laplacian.

The key components include:
- `ConsensusOp`: A base class for consensus operations, with subclasses for Python and C++ implementations.
- `ConsensusProcessor`: A base class for implementing Python-based consensus algorithms.
- Example implementations of consensus processors, such as `ConsensusMaxProcessor` and `ExampleJacobiLaplacianProcessor`.

This module integrates with a C++ framework via the `skywing_cpp_interface` module, which provides
networking and execution support for consensus algorithms.
"""


_skywing_consensus_jobs = {}
_call_site_cache = {}


def get_call_site_key():
    """
    Retrieves a unique key for the current call site in the user code.

    The key is derived from the filename and line number of the code that is
    three levels above the current function call. This is used to uniquely
    identify different invocations of consensus operations.

    Returns:
        Tuple[str, int]: A tuple containing the filename and line number of the call site.
    """
    # Retrieve the user function frame (3 levels above get_call_site_key).
    frame = inspect.currentframe().f_back.f_back.f_back
    # Get the frame information; you could include additional details if needed.
    frame_info = inspect.getframeinfo(frame)
    # Use filename and line number as a simple key.
    return (frame_info.filename, frame_info.lineno)


def get_unique_id_for_call_site():
    """
    Generates or retrieves a unique ID for the current call site.

    This function uses `get_call_site_key` to identify the call site and caches
    the unique IDs for each call site.

    Returns:
        int: A unique integer ID for the call site.
    """
    key = get_call_site_key()
    if key not in _call_site_cache:
        _call_site_cache[key] = len(_call_site_cache)
    return _call_site_cache[key]


def add_unique_id(func):
    """
    Decorator that injects a unique ID into the keyword arguments of the decorated function.

    The unique ID is generated based on the call site of the decorated function.

    Args:
        func (Callable): The function to decorate.

    Returns:
        Callable: The decorated function.
    """

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        # Get or create a unique ID based on the call site.
        unique_id = get_unique_id_for_call_site()
        # Inject the unique ID into the keyword arguments.
        kwargs["unique_id"] = unique_id
        return func(*args, **kwargs)

    return wrapper


def add_agent(func):
    """
    Decorator that injects the calling `Agent` into the keyword
    arguments of the decorated function.

    This decorator traverses the call stack to find the `self` object
    that is an instance of the `Agent` class and adds it to the
    keyword arguments.

    Args:
        func (Callable): The function to decorate.

    Returns:
        Callable: The decorated function.

    """

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        # Find frame where `self` is the Agent that called the job.
        frame = inspect.currentframe()
        while True:
            frame = frame.f_back
            args_info = inspect.getargvalues(frame)
            #            if 'self' in args_info.locals and 'Agent' in str(type(args_info.locals['self'])):
            if "self" in args_info.locals and type(args_info.locals["self"]) == Agent:
                break
        # Get Agent from that frame and attach to kwargs
        agent = args_info.locals["self"]
        kwargs["agent"] = agent
        return func(*args, **kwargs)

    return wrapper


class ConsensusOp(ABC):
    """
    Base class for distributed consensus operations.

    This class defines the interface for creating, updating, and querying consensus jobs.
    Subclasses (e.g., `CppConsensusOp` and `PythonConsensusOp`) implement the specifics
    of how these jobs are created and executed.

    If using a Processor that is defined on the C++ side, use the
    `CppConsensusOp` class; if using a Processor defined in Python,
    use the `PythonConsensusOp` class.

    Consensus operations must be registered prior to use, which can be done via the line
    `my_op_name = CppConsensusOp(cpp_processor_type)`
    or
    `my_op_name = PythonConsensusOp(python_processor_type)`
    . See `skywing.py` for these.

    Abstract Methods:
        make_consensus_job: Creates a new consensus job.
        update_consensus_job: Updates an existing consensus job with new input values.
        query_consensus_job: Queries the result of a consensus job.

    """

    @abstractmethod
    def make_consensus_job(self, uid, *args, **kwargs):
        pass

    @abstractmethod
    def update_consensus_job(self, csj, *args):
        pass

    @abstractmethod
    def query_consensus_job(self, csj):
        pass

    @add_agent
    @add_unique_id
    def __call__(self, *args, **kwargs):
        """Entry point into any consensus operation. Will create a new
        consensus job the first time the operation is used, and will
        update/query the consensus job each subsequent time.

        Note: Each user call to a consensus operation is considered
        UNIQUE and represents a DIFFERENT consensus operation. The
        filename and line number is used to uniquely identify each
        call to a consensus operation in user code (handled via the
        @add_unique_id decorator).

        Args:
           (variadic) Whatever the processor takes as input.

        Returns:
           The current consensus result of the operation.

        """
        uid = kwargs["unique_id"]
        if uid not in _skywing_consensus_jobs:
            csj = self.make_consensus_job(uid, *args, **kwargs)
            _skywing_consensus_jobs[uid] = csj
            csj.submit_to_manager(kwargs["agent"].manager, "consensus_op_" + str(uid))
            return self.query_consensus_job(csj)
        else:
            csj = _skywing_consensus_jobs[uid]
            self.update_consensus_job(csj, *args)
            return self.query_consensus_job(csj)


class CppConsensusOp(ConsensusOp):
    """
    A consensus operation that uses a C++-based processor.

    This class provides a Python interface to consensus algorithms implemented in C++,
    using the `skywing_cpp_interface` module.

    Args:
        consensus_job_type (Type): The type of C++ consensus job to create.

    Methods:
        make_consensus_job: Creates a new C++ consensus job.
        update_consensus_job: Updates the state of the consensus job.
        query_consensus_job: Retrieves the result of the consensus job.
    """

    def __init__(self, consensus_job_type):
        super().__init__()
        self.consensus_job_type = consensus_job_type

    def make_consensus_job(self, uid, *args, **kwargs):
        agent = kwargs["agent"]
        manager = agent.manager
        this_tag = agent.id + "_" + str(uid)
        other_tags = [this_tag] + [
            "agent" + str(nbr.port) + "_" + str(uid) for nbr in agent.nbrs
        ]
        run_duration = 60

        csj = self.consensus_job_type(*args, this_tag, other_tags, run_duration)
        return csj

    def update_consensus_job(self, csj, *args):
        csj.set_value(*args)

    def query_consensus_job(self, csj):
        return csj.get_result()


class PythonConsensusOp(ConsensusOp):
    """
    A consensus operation that uses a Python-based processor.

    This class allows developers to implement consensus algorithms in Python
    by subclassing `ConsensusProcessor` and passing it to this class.

    Args:
        consensus_processor_type (Type[ConsensusProcessor]): The Python processor class to use.

    Methods:
        make_consensus_job: Creates a new Python consensus job.
        update_consensus_job: Updates the state of the consensus job.
        query_consensus_job: Retrieves the result of the consensus job.
    """

    def __init__(self, consensus_processor_type):
        super().__init__()
        self.consensus_processor_type = consensus_processor_type

    def make_consensus_job(self, uid, *args, **kwargs):
        # Collect construction information for collective sum
        agent = kwargs["agent"]
        manager = agent.manager
        this_tag = agent.id + "_" + str(uid)
        other_tags = [this_tag] + [
            "agent" + str(nbr.port) + "_" + str(uid) for nbr in agent.nbrs
        ]
        run_duration = 60

        csj = skywing_cpp_interface.ConsensusJob(
            self, uid, this_tag, other_tags, run_duration
        )
        csj._processor = self.consensus_processor_type(agent, *args)
        csj._processor.set_value(*args)
        return csj

    def update_consensus_job(self, csj, *args):
        csj._processor.set_value(*args)

    def query_consensus_job(self, csj):
        return csj._processor.get_result()

    def process_update(
        self,
        uid: int,
        my_id: str,
        data: Dict[str, Tuple[List[str], List[float], List[int]]],
    ):
        csj = _skywing_consensus_jobs[uid]
        val_data = {}
        for tag, (val_s, val_d, val_i) in data.items():
            val_data[tag] = csj._processor.deconvert(val_s, val_d, val_i)
        csj._processor.process_update(my_id, val_data)

    def prepare_for_publication(self, uid: int):
        csj = _skywing_consensus_jobs[uid]
        result = csj._processor.convert(csj._processor.prepare_for_publication())
        return result


class ConsensusProcessor(ABC):
    """
    Base class for implementing Python-based consensus algorithms.

    Developers should subclass this class to define the local update logic
    for their consensus algorithm.

    Methods:
        set_value: Sets the input value(s) for the processor.
        get_result: Retrieves the current result of the processor.
        process_update: Processes updates received from neighboring agents.
        prepare_for_publication: Prepares the processor's state for sharing with neighbors.
        convert: Converts internal data to an export format (lists of strings, doubles, and ints).
        deconvert: Converts export format data back to internal data.
    """

    def set_value(self, *args):
        if len(args) > 1:
            self.value = args  # store as tuple
        else:
            self.value = args[0]

    def get_result(self):
        return self.result

    @abstractmethod
    def process_update(self, my_id: str, data: Dict[str, Any]):
        pass  # to be implemented in subclass

    @abstractmethod
    def prepare_for_publication(self):
        pass

    @abstractmethod
    def convert(self, input_value):
        # convert list of inputs into
        # list[str], list[double], list[int]
        pass

    @abstractmethod
    def deconvert(self, val_strings, val_doubles, val_ints):
        # convert list[str], list[double], list[int] into data user
        # wants
        pass


class ExampleConsensusMaxProcessor(ConsensusProcessor):
    """
    A processor that computes the maximum value among all agents in the network.

    NOTE: This processor is for example purposes ONLY. This processor
    lacks a Markov consensus, as large stale values will continue to
    permeate. It is recommended to use the C++ max processor for
    practical use cases instead.

    Args:
        agent (Agent): The agent running the processor.
        input_value (float): The initial value for the processor.

    Methods:
        process_update: Updates the maximum value based on data from neighbors.
        prepare_for_publication: Returns the current maximum value for sharing with neighbors.
        convert: Converts the maximum value to the export format.
        deconvert: Converts the export format back to a maximum value.

    """

    def __init__(self, agent, input_value):
        super(ExampleConsensusMaxProcessor, self).__init__()
        self.value = input_value
        self.result = input_value

    def process_update(self, my_id: str, val_data: Dict[str, float]):
        for tag, d in val_data.items():
            if d is not None:
                self.value = d if d > self.value else self.value
        self.result = self.value

    def prepare_for_publication(self):
        return self.value

    def convert(self, input_value: float):
        # convert list of inputs into
        # list[str], list[double], list[int]
        return [], [input_value], []

    def deconvert(self, val_strings, val_doubles, val_ints):
        # convert list[str], list[double], list[int] into data user
        # wants
        if len(val_doubles) > 0:
            return val_doubles[0]
        else:
            return None


class ExampleJacobiLaplacianProcessor(ConsensusProcessor):
    """
    A processor that solves a linear system using the Jacobi
    method based on a graph Laplacian matrix. Each agent is
    responsible for one row of the matrix, and the graph Laplacian
    topology is derived from the Skywing collective's connectivity.

    This processor implements a distributed Jacobi Laplacian algorithm, where
    each agent computes updates to its local value based on data from its neighbors.

    This algorithm DOES have a Markov consensus (I think), so it is
    usable, but it's not the most efficient or robust.

    Args:
        agent (Agent): The agent running the processor.
        diag (float): Constant value added to the matrix diagonal.
        off_diag_mag (float): The magnitude of the off-diagonal Laplacian elements. (Also added to the diagonal as required by the Laplacian).
        rhs (float): This agent's right-hand side value of the linear system.

    Attributes:
        decay_rate (float): A parameter controlling the rate of convergence.

    Methods:
        process_update: Updates the local value based on the Jacobi method.
        prepare_for_publication: Returns the current value for sharing with neighbors.
        convert: Converts the local value to the export format.
        deconvert: Converts the export format back to a local value.

    """

    def __init__(self, agent, diag, off_diag_mag, rhs):
        super(ExampleJacobiLaplacianProcessor, self).__init__()
        self.diag, self.off_diag_mag, self.rhs = diag, off_diag_mag, rhs
        self.x_i = 0.0
        self.result = 0.0
        self.decay_rate = 0.5

    def process_update(self, my_id: str, data: Dict[str, float]):
        self.diag, self.off_diag_mag, self.rhs = self.value

        # Number of neighbors, subtract 1 to not self-count
        num_nbrs = len(data) - 1
        # matrix diagonal is diag offset + off-diagonal * num_nbrs
        a_ii = self.diag + self.off_diag_mag * num_nbrs

        old_x_i = self.x_i
        x_i = self.rhs
        for tag, val in data.items():
            if tag != my_id and val is not None:
                x_i += self.off_diag_mag * val
        x_i = x_i / a_ii
        self.x_i = self.decay_rate * old_x_i + (1 - self.decay_rate) * x_i
        self.result = x_i

    def prepare_for_publication(self):
        return self.x_i

    def convert(self, input_value: float):
        # convert list of inputs into
        # list[str], list[double], list[int]
        return [], [input_value], []

    def deconvert(self, val_strings, val_doubles, val_ints) -> float:
        # convert list[str], list[double], list[int] into data user
        # wants
        if len(val_doubles) > 0:
            return val_doubles[0]
        else:
            return None
