import time
from typing import Optional

from loguru import logger

from .tcp_socket import TCPSocket
from .types import MachineID


class NeighborAgent:
    """Represents a connection to a neighboring agent.

    Each neighbor has:
    - One or more TCP socket connections (communicators)
    - Machine ID
    - Last heartbeat time
    - Connection state

    Supports multiple socket connections per neighbor to handle the case where
    both agents initiate connections to each other, resulting in one
    NeighborAgent with two sockets.
    """

    def __init__(
        self,
        addr: str,
        port: int,
        machine_id: MachineID,
        socket: Optional[TCPSocket] = None,
    ):
        """Initialize neighbor agent.

        Args:
            addr: Neighbor's IP address
            port: Neighbor's port
            machine_id: Unique identifier for neighbor
            socket: Existing TCP socket (from accept()) or None
        """
        self.addr = addr
        self.port = port
        self.machine_id = machine_id

        # TCP connections - list of sockets (communicators)
        self.sockets: list[TCPSocket] = []
        if socket is not None:
            # Socket from incoming connection
            self.sockets.append(socket)
        else:
            # Create new socket for outgoing connection
            self.sockets.append(TCPSocket())

        # Timing and state
        self.last_heard_from = time.monotonic()
        self.is_connected = False

        # Connection type tracking
        self.connection_type = "pending"  # "by_accept", "by_request", "pending"

    def add_communicator(self, socket: TCPSocket) -> None:
        """Add an additional socket (communicator) to this neighbor.

        Called when both sides initiate connections, resulting in
        multiple sockets to the same neighbor.

        Args:
            socket: TCP socket to add
        """
        self.sockets.append(socket)
        logger.debug(
            f"Added communicator to {self.machine_id}, now has {len(self.sockets)} socket(s)"
        )

    def connect(self, timeout: float = 5.0) -> bool:
        """Initiate connection to neighbor.

        Args:
            timeout: Connection timeout

        Returns:
            True if successful, False otherwise
        """
        # Connect using first socket (always created in __init__)
        socket = self.sockets[0]
        success = socket.connect(self.addr, self.port, timeout)
        if success:
            self.is_connected = True
            self.connection_type = "by_request"
            self.last_heard_from = time.monotonic()
        return success

    def send(self, data: bytes) -> bool:
        """Send data to neighbor on first available socket.

        Args:
            data: Bytes to send

        Returns:
            True if sent successfully, False otherwise
        """
        if not self.is_connected:
            return False

        # Try to send on first connected socket
        for socket in self.sockets:
            if socket.is_connected():
                success = socket.send(data)
                if success:
                    return True

        # If all sends failed, mark as disconnected
        self.is_connected = False
        return False

    def send_batch(self, batch: list[bytes]) -> bool:
        """Send data to neighbor on first available socket.

        Args:
            data: Bytes to send

        Returns:
            True if sent successfully, False otherwise
        """
        if not self.is_connected:
            return False

        # Try to send on first connected socket
        for socket in self.sockets:
            if socket.is_connected():
                success = socket.send_batch(batch)
                if success:
                    return True

        # If all sends failed, mark as disconnected
        self.is_connected = False
        return False

    def receive(self) -> Optional[list[bytes]]:
        """Receive data from neighbor (checks all sockets).

        Returns:
            Received list[bytes], or None if no data available
        """
        if not self.is_connected:
            return None

        # Try to receive from any socket
        for socket in self.sockets:
            if not socket.is_connected():
                continue

            data = socket.receive()
            if data is not None:
                self.last_heard_from = time.monotonic()
                return data

        # Check if all sockets are disconnected
        if not any(s.is_connected() for s in self.sockets):
            self.is_connected = False

        return None

    def mark_as_accepted(self) -> None:
        """Mark this connection as coming from accept().

        Called when a neighbor connects to us.
        """
        self.is_connected = True
        self.connection_type = "by_accept"
        self.last_heard_from = time.monotonic()

    def time_since_last_heard(self) -> float:
        """Get time since last message from neighbor.

        Returns:
            Seconds since last heard from
        """
        return time.monotonic() - self.last_heard_from

    def is_timeout(self, timeout_threshold: float) -> bool:
        """Check if neighbor has timed out.

        Args:
            timeout_threshold: Timeout threshold in seconds

        Returns:
            True if timed out, False otherwise
        """
        return self.time_since_last_heard() > timeout_threshold

    def close(self) -> None:
        """Close all connections to neighbor."""
        for socket in self.sockets:
            socket.close()
        self.is_connected = False

    def __repr__(self) -> str:
        status = "connected" if self.is_connected else "disconnected"
        return f"NeighborAgent({self.machine_id}, {self.addr}:{self.port}, {status})"
