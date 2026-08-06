import socket
import threading
from itertools import chain
from typing import Any

from loguru import logger

from .neighbor_agent import NeighborAgent
from .serialization import deserialize, serialize
from .tcp_socket import TCPSocket
from .types import (
    DataMessage,
    HandshakeMessage,
    MachineID,
    MessageBase,
    ProcessorData,
    TagID,
)


class ThreadSafeDict:
    def __init__(self):
        self._dict = {}
        self._lock = threading.Lock()

    def __setitem__(self, key, value):
        with self._lock:
            self._dict[key] = value

    def __getitem__(self, key, default=None):
        with self._lock:
            return self._dict.get(key, default)

    def __delitem__(self, key):
        with self._lock:
            del self._dict[key]

    def __bool__(self):
        with self._lock:
            return bool(self._dict)

    def values(self):
        with self._lock:
            return self._dict.values()

    def keys(self):
        with self._lock:
            return self._dict.keys()

    def items(self):
        with self._lock:
            return self._dict.items()

    def clear(self):
        with self._lock:
            self._dict.clear()


class Manager:
    """Manager using TCP point-to-point connections."""

    def __init__(self, machine_id: MachineID, port: int) -> None:
        """Initialize manager.

        Args:
            machine_id: Unique identifier for this agent
            port: Port to listen on for incoming connections
        """
        self.machine_id = machine_id
        self.port = port

        # Server socket for accepting incoming connections
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind(("0.0.0.0", port))  # Listen on all interfaces
        self.server_socket.listen(
            10
        )  # Allow up to 10 pending connections AM: may need to increase later
        self.server_socket.setblocking(False)  # Non-blocking accept

        # Pending connections (not yet in neighbors map after handshake)
        # Key: (addr, port) - the socket address
        # Value: NeighborAgent waiting for handshake
        self.pending_conns: dict[tuple[str, int], NeighborAgent] = {}

        # Established neighbor tracking: MachineID -> NeighborAgent
        # Key is machine_id sent in handshake
        self.neighbors: dict[MachineID, NeighborAgent] = {}

        # Configured neighbor addresses (to connect to at startup)
        self.initial_neighbor_addresses: list[tuple[str, int]] = []

        # Jobs registered with this manager
        self.jobs: dict[str, Any] = {}

        # Tag subscriptions: tag_id -> list of Jobs subscribed to it
        self.tag_to_jobs: dict[TagID, list[Any]] = {}

        # Tags produced locally
        self.local_tags: set[TagID] = set()

        # Version tracking per tag
        # Each tag has a monotonically increasing version number
        # Key: TagID, Value: current version for that tag
        self.tag_versions: dict[TagID, int] = {}
        self._version_lock = threading.Lock()

        # Running flag
        self._running = False
        self._manager_thread = None

        # Batch sending variables
        self._pending_updates = ThreadSafeDict()

        logger.debug(f"Manager '{machine_id}' initialized, listening on port {port}")

    def configure_initial_neighbors(
        self, neighbors: list[tuple[str, int]], timeout: int = 10
    ) -> None:
        """Configure neighbors to connect to at startup.

        Args:
            neighbors: list of (addr, port) tuples
            timeout: Connection timeout (not used yet, for API compatibility)
        """
        self.initial_neighbor_addresses = neighbors
        logger.debug(f"Configured {len(neighbors)} initial neighbors")

    def _make_neighbor_connections(self) -> None:
        """Initiate connections to configured neighbors.

        Adds connections to pending_conns, not directly to neighbors.
        """
        for addr, port in self.initial_neighbor_addresses:
            conn_key = (addr, port)

            # Skip if already connected or pending, since
            # we do not want duplicated NeighborAgent.
            if conn_key in self.pending_conns:
                continue

            # Create neighbor with temp ID (will be replaced with real machine_id after handshake)
            temp_id = f"pending_{addr}_{port}"  # Placeholder until we learn their real machine_id
            neighbor = NeighborAgent(addr, port, temp_id)
            logger.debug(f"Connecting to {addr}:{port}...")

            if neighbor.connect(timeout=5.0):
                logger.debug(f"✓ Connected to {addr}:{port}")

                # Add to pending connections
                self.pending_conns[conn_key] = neighbor

                # Send handshake to learn their machine_id
                self._send_handshake(neighbor)
            else:
                logger.error(f"✗ Failed to connect to {addr}:{port}")

    def _accept_pending_connections(self) -> None:
        """Accept any pending incoming connections.

        Adds to pending_conns, not directly to neighbors.
        """
        try:
            client_sock, client_addr = self.server_socket.accept()
            logger.debug(f"Accepted connection from {client_addr}")

            # Extract host and port from client address tuple (host, port)
            host = client_addr[0]  # IP address as string (e.g., "127.0.0.1")
            port = client_addr[1]  # Port number as int (e.g., 54321 - ephemeral port)

            # Wrap in TCPSocket
            tcp_sock = TCPSocket(client_sock)

            # Use temp ID until handshake reveals their real machine_id
            temp_id = f"incoming_{host}_{port}"  # Placeholder

            neighbor = NeighborAgent(host, port, temp_id, socket=tcp_sock)
            neighbor.mark_as_accepted()

            # Add to pending connections
            # Use ephemeral port as key (may need to increment if collision)
            conn_key = client_addr  # (addr, port) tuple
            inc_port = client_addr[1]
            while conn_key in self.pending_conns:
                inc_port += 1
                conn_key = (client_addr[0], inc_port)

            self.pending_conns[conn_key] = neighbor
            logger.debug(
                f"Added accepted connection to pending_conns with key {conn_key}"
            )

            # Send handshake back (bidirectional handshake protocol)
            # Both sides send handshakes and wait for greeting responses
            self._send_handshake(neighbor)

        except BlockingIOError:
            # No pending connections (non-blocking)
            pass
        except OSError as e:
            logger.error(f"Accept error: {e}")

    def _send_handshake(self, neighbor: NeighborAgent) -> None:
        """Send handshake message to neighbor.

        HandshakeMessage includes our machine_id so neighbor knows who we are.

        Args:
            neighbor: Neighbor to send handshake to
        """
        try:
            data = serialize(
                HandshakeMessage(remote_id=self.machine_id, port=self.port)
            )
        except TypeError as e:
            logger.warning(
                f"Could not serialize handshake message for transport - invalid type: {e}"
            )
            return

        neighbor.send(data)

    def _handle_handshake(
        self, msg: HandshakeMessage, conn_key: tuple[str, int]
    ) -> None:
        """Handle handshake message from pending connection.

        Promotes connection from pending_conns to neighbors.
        Uses try-insert pattern: if machine_id already exists, adds socket
        to existing neighbor instead of creating duplicate.

        Args:
            msg: HandshakeMessage message
            conn_key: Key in pending_conns map
        """

        # Get the pending connection
        if conn_key not in self.pending_conns:
            logger.warning(f"HandshakeMessage for unknown connection {conn_key}")
            return

        pending_neighbor = self.pending_conns[conn_key]

        # Try to insert into neighbors map
        # Key is remote_id (machine_id from handshake)
        if msg.remote_id not in self.neighbors:
            # New neighbor - add to neighbors map
            # Overwrites temporary name "incoming" or "pending"
            pending_neighbor.machine_id = msg.remote_id
            self.neighbors[msg.remote_id] = pending_neighbor

            logger.debug(f"HandshakeMessage: Added new neighbor '{msg.remote_id}'")
        else:
            # This is a duplicate request, we already have this neighbor
            # Add this socket as additional communicator
            logger.debug(
                f"HandshakeMessage: Already have connection to '{msg.remote_id}', adding communicator"
            )

            existing_neighbor = self.neighbors[msg.remote_id]

            # Move socket from pending to existing neighbor
            # Pending neighbor should have exactly one socket
            if len(pending_neighbor.sockets) > 0:
                socket = pending_neighbor.sockets[0]
                existing_neighbor.add_communicator(socket)

        # Remove from pending connections
        del self.pending_conns[conn_key]

    def submit_job(self, job_id: str, job) -> bool:
        """Register a job with the manager.

        If the manager is already running, the job is started immediately.
        Otherwise, it will be started when launch() is called.

        Args:
            job_id: Unique job identifier
            job: Job instance

        Returns:
            True if successful, False if job_id already exists
        """
        if job_id in self.jobs:
            return False

        self.jobs[job_id] = job
        logger.debug(f"Job '{job_id}' submitted to manager")

        # If manager is already running, start the job immediately
        if self._running:
            logger.debug(f"Starting job '{job_id}' (manager already running)")
            job.run()

        return True

    def job_subscribe(self, job, tags: list[TagID]) -> None:
        """Register job subscriptions.

        Called by Job.subscribe() to inform manager about subscriptions.

        Args:
            job: Job instance subscribing
            tags: list of tag IDs to subscribe to
        """
        for tag in tags:
            if tag not in self.tag_to_jobs:
                self.tag_to_jobs[tag] = []
            if job not in self.tag_to_jobs[tag]:
                self.tag_to_jobs[tag].append(job)

        logger.debug(f"Job '{job.job_id}' subscribed to tags: {tags}")

    def report_new_publish_tags(self, tags: list[TagID]) -> None:
        """Report new tags being produced locally.

        Args:
            tags: list of tag IDs being published
        """
        for tag in tags:
            self.local_tags.add(tag)
        logger.debug(f"Manager now producing tags: {tags}")

    def publish(self, tag: TagID, value: ProcessorData) -> None:
        """Publish data on a tag with version tracking.

        Delivers to local subscribers and adds to the updates dict to send to all connected neighbors via TCP.
        Each message gets a monotonically increasing version number per tag.

        Args:
            tag: Tag to publish on
            value: Value to publish (any child class of ProcessorData)
        """

        # Get and increment version for this tag
        with self._version_lock:
            if tag not in self.tag_versions:
                self.tag_versions[tag] = 0
            else:
                self.tag_versions[tag] += 1
            version = self.tag_versions[tag]

        # First, deliver to local jobs subscribed to this tag
        if tag in self.tag_to_jobs:
            for job in self.tag_to_jobs[tag]:
                try:
                    job.process_data(tag, value, version)
                except Exception as e:
                    logger.error(
                        f"Error delivering local data to job '{job.job_id}' for tag '{tag}': {e}"
                    )

        # Then send to all connected neighbors via TCP

        msg = DataMessage(
            remote_id=self.machine_id, tag=tag, data=value, version=version
        )
        try:
            data = serialize(msg)
            self._pending_updates[tag] = data
        except TypeError as e:
            logger.warning(
                f"Could not serialize data message for transport - invalid type: {e}"
            )

    def _flush_pending_updates(self):
        """Flushes the pending batch buffer."""

        # If we are not done sending the last batch, dont send another yet
        if not self._flush_pending_messages():
            return

        # If there are no pending updates, return
        if not self._pending_updates:
            return

        batch = [msg for msg in self._pending_updates.values()]

        # Send updates to all neighbors
        for neighbor_id, neighbor in list(self.neighbors.items()):
            if neighbor.is_connected:
                success = neighbor.send_batch(batch)
                if not success:
                    logger.warning(f"Failed to send batch to {neighbor_id}")

        self._pending_updates.clear()

    def _handle_message(
        self, msg: MessageBase, conn_key: tuple[str, int], from_pending: bool
    ) -> None:
        """Handle received message.

        Routes message based on type: handshake, data, etc.

        Args:
            msg: Deserialized message dictionary
            conn_key: Connection key (addr, port)
            from_pending: True if from pending_conns, False if from neighbors
        """
        if isinstance(msg, HandshakeMessage):
            # HandshakeMessages should only come from pending connections
            if from_pending:
                self._handle_handshake(msg, conn_key)
            else:
                logger.warning(
                    f"Received handshake from established neighbor {conn_key}"
                )

        elif isinstance(msg, DataMessage):
            # Ignore own messages (shouldn't happen with TCP, but just in case)
            if msg.remote_id == self.machine_id:
                return

            # Deliver to all jobs subscribed to this tag (with version)
            if msg.tag in self.tag_to_jobs:
                for job in self.tag_to_jobs[msg.tag]:
                    try:
                        job.process_data(msg.tag, msg.data, msg.version)
                    except Exception as e:
                        logger.error(
                            f"Error delivering data to job '{job.job_id}' for tag '{msg.tag}': {e}"
                        )

    def _check_connection_progress(self) -> None:
        """Check progress of non-blocking connections

        For pending connections that are still connecting, check their progress
        and send handshake once connected.
        """
        for conn_key, neighbor in list(self.pending_conns.items()):
            # Check each socket's connection progress
            for sock in neighbor.sockets:
                if sock.is_connecting():
                    # Check progress
                    from .tcp_socket import ConnectionState

                    state = sock.connection_progress()

                    if state == ConnectionState.CONNECTED:
                        logger.debug(f"✓ Connection to {conn_key} completed")
                        neighbor.is_connected = True
                        # Send handshake
                        self._send_handshake(neighbor)
                    elif state == ConnectionState.FAILED:
                        logger.error(f"✗ Connection to {conn_key} failed")
                        # Remove from pending
                        del self.pending_conns[conn_key]
                        break

    def _handle_neighbor_messages(self) -> None:
        """Poll all connections (pending and established) for incoming messages.

        Drains all queued messages from:
        1. Pending connections (awaiting handshake completion)
        2. Established neighbors (active communication)
        """
        self._poll_pending_connections()
        self._poll_established_neighbors()

    def _poll_pending_connections(self) -> None:
        """Drain messages from pending connections (expecting handshakes)."""
        for conn_key, neighbor in list(self.pending_conns.items()):
            if neighbor.is_connected:
                self._drain_neighbor_sockets(neighbor, conn_key, from_pending=True)

    def _poll_established_neighbors(self) -> None:
        """Drain messages from established neighbors (data messages)."""
        for _neighbor_id, neighbor in list(self.neighbors.items()):
            if neighbor.is_connected:
                conn_key = (neighbor.addr, neighbor.port)
                self._drain_neighbor_sockets(neighbor, conn_key, from_pending=False)

    def _drain_neighbor_sockets(
        self, neighbor, conn_key: tuple[str, int], from_pending: bool
    ) -> None:
        """Drain all messages from all sockets for a neighbor.

        Iterates through all sockets for a neighbor and drains each one's message queue.

        Args:
            neighbor: NeighborAgent to drain messages from
            conn_key: Connection key for message handling
            from_pending: True if from pending_conns, False if from neighbors
        """
        for sock in neighbor.sockets:
            if sock.is_connected():
                self._drain_socket(sock, conn_key, from_pending)

    def _drain_socket(
        self, sock, conn_key: tuple[str, int], from_pending: bool
    ) -> None:
        """Drain all queued messages from a single socket.

        Args:
            sock: TCPSocket to drain
            conn_key: Connection key for message routing
            from_pending: True if from pending connection (handshake expected)
        """

        data = sock.receive()

        if not data:
            return
        for msg in data:
            try:
                decoded_msg = deserialize(msg)
                self._handle_message(decoded_msg, conn_key, from_pending)
            except ValueError as e:
                source = (
                    f"pending {conn_key}" if from_pending else f"neighbor {conn_key}"
                )
                logger.error(f"Failed to deserialize message from {source}: {e}")

    def _flush_pending_messages(self):
        """Go through all pending and established neighbors and flush the send buffers."""
        all_sent = True
        for neighbor in chain(self.pending_conns.values(), self.neighbors.values()):
            if neighbor.is_connected:
                for sock in neighbor.sockets:
                    if sock.is_connected() and sock.is_sending():
                        sock._flush_send_buffer()
                        if sock.is_sending():
                            all_sent = False
        return all_sent

    def _manager_loop(self) -> None:
        """Main manager event loop.

        Continuously:
        1. Accepts new connections
        2. Polls neighbors for messages
        3. Handles messages
        4. Checks connection progress
        """
        logger.debug(f"Manager '{self.machine_id}' event loop started")

        try:
            while self._running:
                # Accept any pending incoming connections
                self._accept_pending_connections()

                # Check connection progress for pending connections
                self._check_connection_progress()

                # Poll all neighbors for messages
                self._handle_neighbor_messages()

                # Send most recent published updates
                self._flush_pending_updates()

        except KeyboardInterrupt:
            logger.debug(f"\nManager '{self.machine_id}' interrupted")
        finally:
            self._cleanup()
            logger.debug(f"Manager '{self.machine_id}' event loop stopped")

    def _cleanup(self) -> None:
        """Clean up resources on shutdown."""
        # Close all pending connections
        for neighbor in self.pending_conns.values():
            neighbor.close()
        self.pending_conns.clear()

        # Close all established neighbor connections
        for neighbor in self.neighbors.values():
            neighbor.close()
        self.neighbors.clear()

        # Close server socket
        try:
            self.server_socket.close()
        except OSError:
            pass

    def launch(self) -> None:
        """Start the manager event loop.

        This:
        1. Initiates connections to configured neighbors
        2. Starts all jobs
        3. Starts manager event loop in background thread
        """
        if self._running:
            logger.debug("Manager already running")
            return

        self._running = True

        # Connect to configured neighbors
        self._make_neighbor_connections()

        # Start all submitted jobs
        for job_id, job in self.jobs.items():
            logger.debug(f"Starting job '{job_id}'")
            job.run()

        # Start manager event loop in background
        self._manager_thread = threading.Thread(target=self._manager_loop, daemon=True)
        self._manager_thread.start()

        logger.debug(
            f"Manager '{self.machine_id}' started with {len(self.jobs)} jobs and {len(self.neighbors)} neighbors"
        )

    def stop(self) -> None:
        """Stop the manager."""
        self._running = False
        if self._manager_thread:
            self._manager_thread.join(timeout=1.0)
        logger.debug(f"Manager '{self.machine_id}' stopped")

    def join(self) -> None:
        """Wait for manager thread to complete."""
        if self._manager_thread:
            self._manager_thread.join()

    def number_of_neighbors(self) -> int:
        """Get number of connected neighbors.

        Returns:
            Number of neighbors
        """
        return sum(1 for n in self.neighbors.values() if n.is_connected)
