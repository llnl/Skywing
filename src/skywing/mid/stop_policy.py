import time
from abc import ABC, abstractmethod


class StopPolicyBase(ABC):
    """Base class for stop policies."""

    @abstractmethod
    def initialize(self) -> None:
        """Initialize the stop policy state."""
        pass

    @abstractmethod
    def should_stop(self, processor) -> bool:
        """Check if iteration should stop.

        Args:
            processor: The processor being used in the iteration

        Returns:
            True if iteration should stop, False otherwise
        """
        pass


class StopPolicyDefault:
    """Default stop policy based on iteration count and/or time limit.

    Args:
        max_iters: Maximum number of iterations (None for no limit)
        max_time: Maximum time in seconds (None for no limit)
    """

    def __init__(self, **kwargs):
        self.max_iters = kwargs.get("max_iters")
        self.max_time = kwargs.get("max_time")

    def initialize(self) -> None:
        """Initialize iteration counter and start time."""
        self.curr_iters = -1
        self.start_time = time.monotonic()

    def should_stop(self, processor) -> bool:
        """Check if iteration should stop based on count or time.

        Args:
            processor: The processor (not used in default policy)

        Returns:
            True if max iterations or max time exceeded, False otherwise
        """
        self.curr_iters += 1
        self.curr_time = time.monotonic()

        stop = False
        if self.max_iters is not None and self.curr_iters > self.max_iters:
            stop = True
        if (
            self.max_time is not None
            and self.curr_time > self.start_time + self.max_time
        ):
            stop = True

        return stop
