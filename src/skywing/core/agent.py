from collections import namedtuple

from loguru import logger

from .job import Job
from .manager import Manager

Neighbor = namedtuple("Neighbor", ["addr", "port"])


class Agent:
    """Agent wrapper around Manager for distributed communication."""

    def __init__(self, addr: str, port: int) -> None:
        """Initialize agent with address and port.

        Args:
            addr: IP address
            port: Port this agent listens on
        """
        self.addr = addr
        self.port = port
        self.nbrs = []

        machine_id = f"{self.addr}_{self.port}"
        self.manager = Manager(machine_id, self.port)

        logger.debug(f"Agent created at {addr}:{port}")

    def configure_neighbors(
        self, nbrs: list[tuple[str, int]], timeout: int = 10
    ) -> None:
        """Configure neighbor connections.

        This must be called before launch(). Neighbor configuration is a
        separate step from initialization.

        Args:
            nbrs: List of (addr, port) tuples for neighbors
            timeout: Connection timeout (passed to manager)

        Example:
            agent.configure_neighbors([("127.0.0.1", 20001), ("127.0.0.1", 20002)])
        """
        self.nbrs = [Neighbor(addr, port) for addr, port in nbrs]

        # Configure neighbors in manager
        self.manager.configure_initial_neighbors(nbrs, timeout)

        logger.debug(f"Agent configured with neighbors: {self.nbrs}")

    def create_tag_from_uid(self, uid, nbr=None) -> str:
        """Create a tag from a unique ID.

        Args:
            uid: Unique identifier
            nbr: Optional neighbor (Neighbor namedtuple with addr, port)

        Returns:
            Tag string in format "addr_port_uid"
        """
        if nbr is None:
            return f"{self.addr}_{self.port}_{uid}"
        else:
            return f"{nbr.addr}_{nbr.port}_{uid}"

    def submit_job(self, job_name: str, iteration) -> None:
        """Submit an iteration job to the manager.

        Args:
            job_name: Unique identifier for the job
            iteration: Iteration object with a run() method
        """

        # Create job with iteration's run method
        job = Job(job_name, self.manager, iteration.run)
        self.manager.submit_job(job_name, job)

    def launch(self) -> None:
        """Start the manager."""
        # Start the manager (it will run in background thread)
        self.manager.launch()
        logger.debug(f"Agent {self.addr}:{self.port} launched")
