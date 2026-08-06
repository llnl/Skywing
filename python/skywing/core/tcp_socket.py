import errno
import select
import socket
import struct
import time
from enum import Enum
from typing import Optional

from loguru import logger


class ConnectionState(Enum):
    """Connection state for non-blocking connections."""

    DISCONNECTED = 0
    CONNECTING = 1
    CONNECTED = 2
    FAILED = 3


class TCPSocket:
    """TCP socket wrapper for reliable communication.

    Provides a simple interface for TCP connections with non-blocking
    receive and automatic connection management.
    """

    # Constants for buffer and message size management
    CHUNK_SIZE = 4 * 1024  # 4 KiB per read
    ALLOCATION_SIZE = 64 * 1024  # 64 KiB per allocation
    MAX_MESSAGE_SIZE = (
        10 * 1024 * 1024
    )  # 10 MB limit, for framed messages not the raw message
    HEADER_SIZE = 4
    MAX_SEND_QUEUE_SIZE = 1000

    def __init__(self, sock: Optional[socket.socket] = None):
        """Initialize TCP socket.

        Args:
            sock: Existing socket (from accept()) or None to create new
        """
        if sock is not None:
            # Socket from accept() - already connected
            self.sock = sock
            self._state = ConnectionState.CONNECTED
        else:
            # New socket for connect()
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._state = ConnectionState.DISCONNECTED

        # Always non-blocking
        self.sock.setblocking(False)

        # Receive buffer for partial messages
        self._recv_buffer = bytearray()
        # Send buffer for partial sends
        self._send_queue: list[bytes] = []
        self._send_buffer = bytearray()
        self._offset = 0
        self._sending = False

        # Track connection target and start time
        self._target_addr: Optional[tuple[str, int]] = None
        self._connect_start_time: Optional[float] = None
        self._connect_timeout: float = 5.0

    def connect_non_blocking(
        self, addr: str, port: int, timeout: float = 5.0
    ) -> ConnectionState:
        """Initiate non-blocking connection to remote host.

        This method never blocks. Call connection_progress() to check status.

        Args:
            addr: IP address to connect to
            port: Port to connect to
            timeout: Connection timeout in seconds

        Returns:
            ConnectionState indicating current connection status
        """
        if self._state == ConnectionState.CONNECTED:
            return ConnectionState.CONNECTED

        if self._state == ConnectionState.CONNECTING:
            # Already in progress
            return ConnectionState.CONNECTING

        # Start new connection
        self._target_addr = (addr, port)
        self._connect_timeout = timeout
        self._connect_start_time = time.time()

        try:
            # Attempt connection (will immediately return EINPROGRESS)
            self.sock.connect((addr, port))
            # If we get here without exception, connection succeeded immediately
            self._state = ConnectionState.CONNECTED
            return ConnectionState.CONNECTED
        except BlockingIOError:
            # Expected: connection in progress
            self._state = ConnectionState.CONNECTING
            return ConnectionState.CONNECTING
        except OSError as e:
            if e.errno == errno.EINPROGRESS:
                # Expected: connection in progress
                self._state = ConnectionState.CONNECTING
                return ConnectionState.CONNECTING
            # Immediate failure
            self._state = ConnectionState.FAILED
            return ConnectionState.FAILED

    def connection_progress(self) -> ConnectionState:
        """Check progress of non-blocking connection.

        Returns:
            ConnectionState indicating current status
        """
        if self._state != ConnectionState.CONNECTING:
            return self._state

        # Check timeout
        if self._connect_start_time is not None:
            elapsed = time.time() - self._connect_start_time
            if elapsed > self._connect_timeout:
                self._state = ConnectionState.FAILED
                return ConnectionState.FAILED

        # Use select to check if socket is writable (connection complete)
        try:
            _, writable, exceptional = select.select([], [self.sock], [self.sock], 0)

            if exceptional:
                # Connection failed
                self._state = ConnectionState.FAILED
                return ConnectionState.FAILED

            if writable:
                # Socket is writable - check for errors
                error = self.sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
                if error == 0:
                    # Success!
                    self._state = ConnectionState.CONNECTED
                    return ConnectionState.CONNECTED
                # Connection failed with error
                self._state = ConnectionState.FAILED
                return ConnectionState.FAILED
            # Still in progress
            return ConnectionState.CONNECTING

        except OSError:
            self._state = ConnectionState.FAILED
            return ConnectionState.FAILED

    def connect(self, addr: str, port: int, timeout: float = 5.0) -> bool:
        """Connect to remote host (legacy blocking interface for compatibility).

        This maintains backward compatibility but internally uses non-blocking approach.

        Args:
            addr: IP address to connect to
            port: Port to connect to
            timeout: Connection timeout in seconds

        Returns:
            True if connection successful, False otherwise
        """
        state = self.connect_non_blocking(addr, port, timeout)

        if state == ConnectionState.CONNECTED:
            return True

        # Wait for connection to complete
        start = time.time()
        while time.time() - start < timeout:
            state = self.connection_progress()

            if state == ConnectionState.CONNECTED:
                return True
            if state == ConnectionState.FAILED:
                return False

            # Small sleep to avoid busy loop
            time.sleep(0.0001)  # 100 microseconds

        # Timeout
        self._state = ConnectionState.FAILED
        return False

    def _frame_message(self, data: bytes) -> bytes:
        """Adds framing to data message"""
        return struct.pack("!I", len(data)) + data

    def send_batch(self, batch: list[bytes]):

        if len(batch) == 0:
            return True

        if self._state != ConnectionState.CONNECTED:
            logger.error("Send failed: socket is not connected.")
            return False

        combined_batch = b""
        queued = False

        for msg in batch:
            framed_msg = self._frame_message(msg)
            # If the batch is too large, queue it as multiple messages
            if (len(combined_batch) + len(framed_msg)) > self.MAX_MESSAGE_SIZE:
                # Don't queue empty batches
                if combined_batch != b"":
                    self._queue_message(combined_batch)
                    queued = True
                combined_batch = b""

            # Check the individual message for size compatibility
            if len(framed_msg) > self.MAX_MESSAGE_SIZE:
                logger.warning(
                    f"Batch partially failed: message size {len(msg)} exceeds limit {self.MAX_MESSAGE_SIZE} bytes, payload cannot be larger than {self.MAX_MESSAGE_SIZE - self.HEADER_SIZE} bytes. Message dropped."
                )
                continue

            combined_batch += framed_msg

        if combined_batch != b"":
            self._queue_message(combined_batch)
            queued = True

        self._flush_send_buffer()
        return queued

    def send(self, data: bytes) -> bool:
        """Send data with length prefix.

        Format: [4-byte length][data]

        Args:
            data: Bytes to send

        Returns:
            True if sent successfully, False otherwise
        """

        if self._state != ConnectionState.CONNECTED:
            return False

        # Validate message size
        framed_msg = self._frame_message(data)
        if len(framed_msg) > self.MAX_MESSAGE_SIZE:
            logger.error(
                f"Send failed: message size {len(framed_msg)} exceeds limit {self.MAX_MESSAGE_SIZE} bytes, payload cannot be larger than {self.MAX_MESSAGE_SIZE - self.HEADER_SIZE} bytes"
            )
            return False

        # Queue the framed message
        queued = self._queue_message(framed_msg)
        self._flush_send_buffer()

        return queued

    def _queue_message(self, data: bytes):
        """Queue pending message in the send queue."""

        # If queue is at max size, drop oldest queued msg
        if len(self._send_queue) >= self.MAX_SEND_QUEUE_SIZE:
            dropped = self._send_queue.pop(0)  # Remove oldest
            logger.warning(
                f"Send queue full ({self.MAX_SEND_QUEUE_SIZE}), dropped oldest message ({len(dropped)} bytes)"
            )

        self._send_queue.append(data)

        return True

    def _flush_send_buffer(self):
        """Attempts to flush the send buffer and send queue. Will send as much data as possible."""

        # If there is data to send or data queued, loop on the send.
        while len(self._send_queue) > 0 or self._sending:
            try:
                # If no data being currently sent, start sending the next message in the queue.
                if not self._sending:
                    # If no messages in the queue, break out of while loop
                    if len(self._send_queue) == 0:
                        break
                    self._sending = True
                    self._offset = 0
                    self._send_buffer = self._send_queue.pop(0)

                # Send as much of the current message as you can
                sent = self.sock.send(self._send_buffer[self._offset :])

                self._offset += sent

                # Check if the full message was sent, and if it was, reset the send_buffer
                if self._offset >= len(self._send_buffer):
                    self._send_buffer = bytearray()
                    self._offset = 0
                    self._sending = False

            except BlockingIOError:
                # Socket is busy, cannot send anymore data right now.
                return
            except OSError as e:
                # Sometimes python will throw generic error instead of Blocking IO error, check error type
                if e.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                    # Socket is busy, cannot send anymore data right now.
                    return
                logger.error(f"Send failed, connection lost: {e}")
                self.close()
                return

    def receive(self) -> Optional[list[bytes]]:
        """Receive data (non-blocking).

        Expects data with length prefix: [4-byte length][data]
        Uses persistent buffer to handle partial messages

        Returns:
            Received bytes, or None if no complete message available
        """
        if self._state != ConnectionState.CONNECTED:
            return None
        try:
            self._recv_all_available()
            return self._grab_all_available_messages()
        except (ConnectionResetError, OSError) as e:
            logger.error(f"Receive failed: {e}")
            self._state = ConnectionState.FAILED
            return None

    def _recv_all_available(self):
        """Reads all available data from the socket into the internal recv buffer for processing."""
        chunk = True
        try:
            # Receive data until no more data left to receive, or connection drops
            while chunk:
                # Try to read more data
                chunk = self.sock.recv(self.CHUNK_SIZE)
                if not chunk:
                    # Connection closed by peer, close out socket and disconnect.
                    self.close()
                    return
                self._recv_buffer.extend(chunk)
        except BlockingIOError:
            # No more data available right now, receiving complete.
            return

    def _grab_all_available_messages(self) -> Optional[list[bytes]]:
        """Process all complete messages in the receive buffer."""
        messages = []
        while True:
            msg = self._grab_next_message()
            if msg is None:
                break
            messages.append(msg)
        if len(messages) > 0:
            return messages
        return None

    def _grab_next_message(self) -> Optional[bytes]:
        """Grabs the next message off of the internal receive buffer."""
        # Full header has not been received yet, no next message to return yet.
        if len(self._recv_buffer) < self.HEADER_SIZE:
            return None
        total_bytes = struct.unpack("!I", self._recv_buffer[: self.HEADER_SIZE])[0]
        # Validate data size
        if total_bytes > self.MAX_MESSAGE_SIZE - self.HEADER_SIZE:
            logger.error(
                f"Receive failed: message size {total_bytes} exceeds limit {self.MAX_MESSAGE_SIZE - self.HEADER_SIZE}"
            )
            self._state = ConnectionState.FAILED
            self.close()
            return None
        # Full message has not been received yet, no next message to return yet.
        if len(self._recv_buffer[self.HEADER_SIZE :]) < total_bytes:
            return None

        data = bytes(
            self._recv_buffer[self.HEADER_SIZE : total_bytes + self.HEADER_SIZE]
        )
        del self._recv_buffer[: total_bytes + self.HEADER_SIZE]
        return data

    def is_connected(self) -> bool:
        """Check if socket is connected.

        Returns:
            True if connected, False otherwise
        """
        return self._state == ConnectionState.CONNECTED

    def is_connecting(self) -> bool:
        """Check if connection is in progress.

        Returns:
            True if connecting, False otherwise
        """
        return self._state == ConnectionState.CONNECTING

    def is_sending(self) -> bool:
        """Check if msg is currently being sent.

        Returns:
            True if sending, False otherwise
        """
        return self._sending

    def close(self) -> None:
        """Close the socket."""
        try:
            self.sock.close()
        except OSError:
            pass
        self._state = ConnectionState.DISCONNECTED
        self._recv_buffer.clear()
        self._send_queue.clear()
        self._sending = False
        self._offset = 0
        self._send_buffer = bytearray()

    def get_peer_addr(self) -> Optional[tuple[str, int]]:
        """Get peer address.

        Returns:
            (addr, port) tuple or None if not connected
        """
        if self._state != ConnectionState.CONNECTED:
            return None

        try:
            return self.sock.getpeername()
        except OSError:
            return None

    def fileno(self) -> int:
        """Return underlying socket information. Used with select.select"""
        return self.sock.fileno()
