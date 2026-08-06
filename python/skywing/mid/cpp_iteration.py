from abc import ABC, abstractmethod

import skywing.skywing_bind_interface as skywing_bind_interface
from skywing.mid.utils import add_unique_id, check_for_uid, get_job_by_uid, register_uid


class CppIterationBase(ABC):
    """Base class for wrappers around the C++ `AsynchronousIterative` and `SynchronousIterative` classes.

    PORT: The base class is only needed to support C++ processors. When this is no longer needed,
    IterationBase and Iteration can be merged.

    This class defines the interface for creating, updating, and querying jobs.
    Subclasses (e.g., `CppIteration_CppProc` and `CppIteration_PythonProc`) implement the specifics
    of how these jobs are created and executed.

    If using a Processor that is defined on the C++ side, use the
    `CppIteration_CppProc` class; if using a Processor defined in Python,
    use the `CppIteration_PythonProc` class.

    Jobs must be registered prior to use, which can be done via the line
    `my_op_name = CppIteration_CppProc(cpp_processor_type)`
    or
    `my_op_name = CppIteration_PythonProc(python_processor_type)`

    Abstract Methods:
        make_job: Creates a new job.
        update_data: Updates an existing job with new input values.
        query: Queries the result of a job.

    """

    @add_unique_id
    def __init__(self, **kwargs):
        self.uid = kwargs["unique_id"]

    @abstractmethod
    def make_job(self, *args, **kwargs):
        pass

    @abstractmethod
    def update_data(self, *args):
        pass

    @abstractmethod
    def query(self):
        pass

    def launch(self, *args, **kwargs) -> None:
        """Create a new job and submit to the agent's manager.

        Note: Each user call to a iteraiton/algorithm is considered
        UNIQUE and represents a DIFFERENT operation. The filename and
        line number is used to uniquely identify each call to an iteration
        in user code (handled via the @add_unique_id decorator).

        Args:
           (variadic) Whatever the processor takes as input.

        """
        if check_for_uid(self.uid):
            raise RuntimeError(
                "ERROR: iteration already launched. To launch a new iteration, instantiate another Iteration object."
            )
        else:
            csj = self.make_job(*args, **kwargs)

            register_uid(self.uid, csj)
            csj.submit_to_manager(self.agent.manager, "job_" + str(self.uid))
            self.agent.launch()

    def wait(self) -> None:
        # WM: todo - this waits on the manager thread to join. Is there a way to
        # just wait on the specific iteration job instead of the manager thread?
        self.agent.manager_thread.join()


class CppIteration_CppProc(CppIterationBase):
    """
    CppIteration that uses a C++-based processor.
    PORT: Eventually, this class will be removed.

    This class provides a Python interface to algorithms implemented in C++,
    using the `skywing_bind_interface` module.

    Args:
        iteration_type (Type): The type of C++ job to create.

    Methods:
        make_job: Creates a new C++ job.
        update: Updates the state of the job.
        query: Retrieves the result of the job.

    """

    def __init__(self, cpp_job_type, agent):
        super().__init__()
        self.cpp_job_type = cpp_job_type
        self.agent = agent

    def make_job(self, *args, **kwargs):
        my_tag = str(self.agent.addr) + "_" + str(self.agent.port) + "_" + str(self.uid)
        other_tags = [my_tag] + [
            str(nbr.addr) + "_" + str(nbr.port) + "_" + str(self.uid)
            for nbr in self.agent.nbrs
        ]
        run_duration = kwargs.get("timeout", 10)
        synchronous = kwargs.get("synchronous", False)

        csj = self.cpp_job_type(*args, my_tag, other_tags, synchronous, run_duration)
        return csj

    def update_data(self, *args) -> None:
        if not check_for_uid(self.uid):
            raise RuntimeError(
                "ERROR: calling update_data() for an iteration that has not yet been launched. Call launch() first."
            )
        else:
            csj = get_job_by_uid(self.uid)
            csj.update_data(*args)

    def query(self):
        if not check_for_uid(self.uid):
            raise RuntimeError(
                "ERROR: calling query() for an iteration that has not yet been launched. Call launch() first."
            )
        else:
            csj = get_job_by_uid(self.uid)
            return csj.get_result()


class CppIteration_PythonProc(CppIterationBase):
    """
    CppIteration that uses a Python-based processor.

    This class allows developers to implement algorithms in Python
    by subclassing `ConsensusProcessor` and passing it to this class.

    Args:
        processor_type (Type[ConsensusProcessor]): The Python processor class to use.

    Methods:
        make_job: Creates a new Python job.
        update: Updates the state of the job.
        query: Retrieves the result of the job.

    """

    def __init__(self, processor_type, agent):
        super().__init__()
        self.processor_type = processor_type
        self.agent = agent

    def make_job(self, *args, **kwargs):
        # Collect construction information for collective sum
        my_tag = str(self.agent.addr) + "_" + str(self.agent.port) + "_" + str(self.uid)
        other_tags = [my_tag] + [
            str(nbr.addr) + "_" + str(nbr.port) + "_" + str(self.uid)
            for nbr in self.agent.nbrs
        ]
        run_duration = kwargs.get("timeout", 10)
        synchronous = kwargs.get("synchronous", False)

        # PORT: Replace call to C++ BindIteration object with python implementation.
        csj = skywing_bind_interface.CppIteration_PythonProc(
            self, self.uid, my_tag, other_tags, synchronous, run_duration
        )
        kwargs["agent"] = self.agent
        kwargs["unique_id"] = self.uid
        csj._processor = self.processor_type(*args, **kwargs)
        return csj

    def update_data(self, *args) -> None:
        if not check_for_uid(self.uid):
            raise RuntimeError(
                "ERROR: calling update_data() for an iteration that has not yet been launched. Call launch() first."
            )
        else:
            csj = get_job_by_uid(self.uid)
            csj._processor.update_data(*args)

    def query(self):
        if not check_for_uid(self.uid):
            raise RuntimeError(
                "ERROR: calling query() for an iteration that has not yet been launched. Call launch() first."
            )
        else:
            csj = get_job_by_uid(self.uid)
            return csj._processor.query()

    def process_update(
        self,
        my_tag: str,
        data: dict[str, tuple[list[str], list[float], list[int]]],
    ) -> None:
        csj = get_job_by_uid(self.uid)
        val_data = {}
        for tag, (val_s, val_d, val_i) in data.items():
            val_data[tag] = csj._processor.deconvert(val_s, val_d, val_i)
        csj._processor.process_update(my_tag, val_data)

    def prepare_for_publication(self):
        csj = get_job_by_uid(self.uid)  # _skywing_jobs[self.uid]
        result = csj._processor.convert(csj._processor.prepare_for_publication())
        return result
