import random
import time
from typing import Any, Union

from skywing.core.agent import Agent
from skywing.core.agent_cpp import AgentCpp
from skywing.core.manager import Manager
from skywing.core.types import MachineID, TagID
from skywing.mid.base_processor import Processor
from skywing.mid.iteration import Iteration
from skywing.mid.iteration_cpp_core import IterationCppCore


def create_skywing_agent(
    address: str,
    port: int,
    nbr_addresses: list[str],
    nbr_ports: list[int],
    test_async: list[float],
    backend: str,
):
    """Utility function for creating a Skywing agent with additional
    flexibility to choose the backend (python or cpp) and to create
    a SlowAgent with artificial slow-downs in the communication.
    """

    # Use local host if no address provided
    if not address:
        address = "127.0.0.1"
    if not nbr_addresses:
        nbr_addresses = ["127.0.0.1" for i in nbr_ports]

    # Parse artificial asynchrony options
    is_slow_agent = False
    if test_async is not None:
        is_slow_agent = test_async[0]
        percent_slow_iterations = test_async[1]
        slow_down = test_async[2]

    if backend == "cpp":
        if is_slow_agent:
            raise RuntimeError(
                "Artificial asynchrony is not available with the cpp backend."
            )
        skywing_agent = AgentCpp(address, port)

    else:
        if is_slow_agent:
            skywing_agent = SlowAgent(address, port, percent_slow_iterations, slow_down)
        else:
            skywing_agent = Agent(address, port)

    nbrs = [(a, p) for (a, p) in zip(nbr_addresses, nbr_ports)]
    skywing_agent.configure_neighbors(nbrs)
    return skywing_agent


def create_skywing_iteration(
    processor: Processor, agent: Union[Agent, AgentCpp], backend: str = "python"
):
    """Create Iteration with the appropriate backend."""
    if backend == "cpp":
        return IterationCppCore(processor, agent)
    else:
        return Iteration(processor, agent)


class SlowManager(Manager):
    """Light wrapper around the Manager class to introduce artificial slow downs.
    Used underneath the SlowAgent class. See SlowAgent for more detail.
    """

    def __init__(
        self,
        machine_id: MachineID,
        port: int,
        percent_slow_iterations: float,
        slow_down: float,
    ):
        super().__init__(machine_id, port)
        self.percent_slow_iterations = percent_slow_iterations
        self.slow_down = slow_down

    def publish(self, tag: TagID, value: Any) -> None:
        # Artificial slowdown
        if random.random() <= self.percent_slow_iterations:
            # Deterministic slow down
            if self.slow_down >= 0.0:
                time.sleep(self.slow_down)
            # Stochastic slow down
            else:
                time.sleep(random.uniform(0.0, -self.slow_down))
        super().publish(tag, value)


class SlowAgent(Agent):
    """Light wrapper around the Agent class to introduce artificial slow downs.
    Slow down is parameterized by:
        - percent_slow_iterations: float between 0.0 and 1.0 describing the
          percentage of iterations that get slowed down.
        - slow_down: float indicating the extra time introduced for the slow down
          in seconds. If a positive value is passed, slow_down seconds are added
          to each iteration just before publishing new data. If a negative value
          is passed, the slow down is randomly sampled from a uniform distribution
          between 0 and (-slow_down) seconds.
    """

    def __init__(
        self, addr: str, port: int, percent_slow_iterations: float, slow_down: float
    ):
        super().__init__(addr, port)
        machine_id = f"{self.addr}_{self.port}"
        self.manager._cleanup()
        self.manager = SlowManager(
            machine_id, self.port, percent_slow_iterations, slow_down
        )
