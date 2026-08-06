"""Test suite for pure Python TCPSocket implementation

Tests the core functionality and guarantees of the TCPSocket implementatio.
"""

import errno
import os
import socket
import threading
import time

import pytest

from skywing.core.tcp_socket import ConnectionState, TCPSocket

# Unit Tests


class TestTCPSocket:
    """Collection of unit tests for the TCPSocket class logic."""

    @pytest.fixture(autouse=True)
    def mock_socket(self, mocker):
        """Mock the socket class to avoid opening an actual socket during testing."""
        self.mock_socket_class = mocker.patch("socket.socket")
        self.mock_socket = self.mock_socket_class.return_value

    def test_initialization(self):

        # Test initialization with no socket passed
        sock = TCPSocket()
        assert sock.sock == self.mock_socket
        assert sock._send_queue == []
        assert sock._recv_buffer == bytearray()
        assert sock._send_buffer == bytearray()
        # Offset should be at 0
        assert sock._offset == 0
        # Sending should be False and the state disconnected
        assert not sock._sending
        assert sock._state == ConnectionState.DISCONNECTED

        # Test initialization with sock as arg
        sock = TCPSocket(self.mock_socket)  # type: ignore
        assert sock.sock == self.mock_socket
        assert sock._send_queue == []
        assert sock._recv_buffer == bytearray()
        assert sock._send_buffer == bytearray()
        # Offset should be at 0
        assert sock._offset == 0
        # Sending should be False and the state disconnected
        assert not sock._sending
        assert sock._state == ConnectionState.CONNECTED

        assert sock._connect_timeout == 5.0

    def test_connect_non_blocking(self):
        sock = TCPSocket()

        # Test successful connection (no execeptions thrown)
        sock.connect_non_blocking("127.0.0.1", 20000)
        assert sock._state == ConnectionState.CONNECTED

        # Reset to disconnected state
        sock._state = ConnectionState.DISCONNECTED

        # Test connection pending (BlockingIOError thrown)
        self.mock_socket.connect.side_effect = BlockingIOError()
        sock.connect_non_blocking("127.0.0.1", 20000)
        assert sock._state == ConnectionState.CONNECTING

        # Reset to disconnected state
        sock._state = ConnectionState.DISCONNECTED

        # Test connection pending (OSError errno.EINPROGRESS thrown)
        self.mock_socket.connect.side_effect = OSError(
            errno.EINPROGRESS, os.strerror(errno.EINPROGRESS)
        )
        sock.connect_non_blocking("127.0.0.1", 20000)
        assert sock._state == ConnectionState.CONNECTING

        # Reset to disconnected state
        sock._state = ConnectionState.DISCONNECTED

        # Test connection failed (OSError thrown)
        self.mock_socket.connect.side_effect = OSError()
        sock.connect_non_blocking("127.0.0.1", 20000)
        assert sock._state == ConnectionState.FAILED

    def test_connection_progress(self, mocker):
        sock = TCPSocket()
        assert sock.connection_progress() == ConnectionState.DISCONNECTED

        # Test exceptional path
        sock._state = ConnectionState.CONNECTING
        mocker.patch("select.select", return_value=(False, False, True))
        assert sock.connection_progress() == ConnectionState.FAILED

        # Test writable path, error = 0
        sock._state = ConnectionState.CONNECTING
        mocker.patch("select.select", return_value=(False, True, False))
        self.mock_socket.getsockopt.return_value = 0
        assert sock.connection_progress() == ConnectionState.CONNECTED

        # Test writable path, error != 0
        sock._state = ConnectionState.CONNECTING
        mocker.patch("select.select", return_value=(False, True, False))
        self.mock_socket.getsockopt.return_value = 1
        assert sock.connection_progress() == ConnectionState.FAILED

        # Test select all false path
        sock._state = ConnectionState.CONNECTING
        mocker.patch("select.select", return_value=(False, False, False))
        assert sock.connection_progress() == ConnectionState.CONNECTING

        # Test select all false path
        sock._state = ConnectionState.CONNECTING
        mocker.patch("select.select", side_effect=OSError())
        assert sock.connection_progress() == ConnectionState.FAILED

    def test_send_batch_success(self, mocker):
        sock = TCPSocket()
        mocker.patch.object(sock, "_flush_send_buffer")

        num_messages = 10
        batch = [str(i).encode() for i in range(num_messages)]

        # Test send_batch when socket not connected
        assert not sock.send_batch(batch)

        # Fake connection to test the rest of the logic
        sock._state = ConnectionState.CONNECTED

        # Test full batch added to the send buffer
        assert sock.send_batch(batch)
        assert len(sock._send_queue) == 1
        assert len(sock._send_queue[0]) == 50

    def test_send_batch_too_large(self, mocker):
        sock = TCPSocket()
        mocker.patch.object(sock, "_flush_send_buffer")

        num_messages = 10
        batch = [str(i).encode() for i in range(num_messages)]

        # Fake connection to test the rest of the logic
        sock._state = ConnectionState.CONNECTED

        # Test batch too large, added to queue separately
        sock.MAX_MESSAGE_SIZE = 40
        assert sock.send_batch(batch)
        assert len(sock._send_queue) == 2
        assert len(sock._send_queue[0]) == 40
        assert len(sock._send_queue[1]) == 10

    def test_send_batch_single_msg_too_large(self, mocker):
        sock = TCPSocket()
        mocker.patch.object(sock, "_flush_send_buffer")

        num_messages = 10
        batch = [str(i).encode() for i in range(num_messages)]
        large_msg = b"".join([str(i).encode() for i in range(1000)])
        batch.insert(5, large_msg)

        # Fake connection to test the rest of the logic
        sock._state = ConnectionState.CONNECTED

        # Adjust the max message size for the test
        sock.MAX_MESSAGE_SIZE = 40
        assert sock.send_batch(batch)
        # Large message should have been dropped
        assert len(sock._send_queue) == 2
        assert len(sock._send_queue[0]) == 25
        assert len(sock._send_queue[1]) == 25

    def test_send_batch_all_msg_max_size(self, mocker):
        sock = TCPSocket()
        mocker.patch.object(sock, "_flush_send_buffer")

        num_messages = 10
        large_msg = b"".join([str(i).encode() for i in range(10)])
        batch = [large_msg for _ in range(num_messages)]
        assert len(batch) == 10

        # Fake connection to test the rest of the logic
        sock._state = ConnectionState.CONNECTED

        # Adjust the max message size for the test
        sock.MAX_MESSAGE_SIZE = len(large_msg) + 4
        assert sock.send_batch(batch)
        # Large message should have been dropped
        assert len(sock._send_queue) == 10
        for msg in sock._send_queue:
            assert len(msg) == len(large_msg) + 4  # add 4 for the framing

    def test_send_batch_all_msg_too_large(self, mocker):
        sock = TCPSocket()
        mocker.patch.object(sock, "_flush_send_buffer")

        num_messages = 10
        large_msg = b"".join([str(i).encode() for i in range(10)])
        batch = [large_msg for _ in range(num_messages)]
        assert len(batch) == 10

        # Fake connection to test the rest of the logic
        sock._state = ConnectionState.CONNECTED

        # Adjust the max message size for the test
        sock.MAX_MESSAGE_SIZE = 5
        assert not sock.send_batch(batch)
        # Large message should have been dropped
        assert len(sock._send_queue) == 0

    def test_send_success(self, mocker):
        sock = TCPSocket()
        mocker.patch.object(sock, "_flush_send_buffer")

        large_msg = b"".join([str(i).encode() for i in range(1000)])

        # Fake connection to test the rest of the logic
        sock._state = ConnectionState.CONNECTED

        # Adjust the max message size for the test
        sock.MAX_MESSAGE_SIZE = len(large_msg) + 4
        assert sock.send(large_msg)
        assert len(sock._send_queue) == 1
        assert len(sock._send_queue[0]) == len(large_msg) + 4

    def test_send_fail_with_large_message(self, mocker):
        sock = TCPSocket()
        mocker.patch.object(sock, "_flush_send_buffer")

        large_msg = b"".join([str(i).encode() for i in range(1000)])

        # Fake connection to test the rest of the logic
        sock._state = ConnectionState.CONNECTED

        # Adjust the max message size for the test
        sock.MAX_MESSAGE_SIZE = 10
        assert not sock.send(large_msg)
        assert len(sock._send_queue) == 0

    def test_queue_message_drop_oldest(self):
        sock = TCPSocket()

        for i in range(sock.MAX_SEND_QUEUE_SIZE + 1):
            sock._queue_message(str(i).encode())

        assert sock._send_queue[0] == str(1).encode()
        assert sock._send_queue[-1] == str(sock.MAX_SEND_QUEUE_SIZE).encode()
        assert len(sock._send_queue) == sock.MAX_SEND_QUEUE_SIZE

    def test_flush_send_buffer_full_queue_send(self):
        sock = TCPSocket()
        sock._state = ConnectionState.CONNECTED

        num_messages = 10
        messages = [str(i).encode() for i in range(num_messages)]

        # Mock sending of all 10 messages
        msg_size = len(sock._frame_message(messages[0]))
        self.mock_socket.send.side_effect = [msg_size for _ in range(num_messages)]

        for msg in messages:
            sock._queue_message(sock._frame_message(msg))

        sock._flush_send_buffer()

        assert len(sock._send_queue) == 0
        assert len(sock._send_buffer) == 0
        assert sock._offset == 0
        assert not sock._sending
        assert sock._state == ConnectionState.CONNECTED

    def test_flush_send_buffer_partial_send_finish(self):
        sock = TCPSocket()
        sock._state = ConnectionState.CONNECTED

        # Mock sending of the second half of the message
        msg = sock._frame_message(str(10).encode())
        msg_size = len(msg)
        self.mock_socket.send.side_effect = [msg_size - msg_size // 2]

        sock._send_buffer = msg
        sock._sending = True
        sock._offset = msg_size // 2
        sock._flush_send_buffer()

        # Send queue should still have no messages
        assert len(sock._send_queue) == 0
        # Offset should be 0
        assert sock._offset == 0
        # Sending should be True and the state connected
        assert not sock._sending
        assert sock._state == ConnectionState.CONNECTED

    def test_flush_send_buffer_partial_queue_send(self):
        sock = TCPSocket()
        sock._state = ConnectionState.CONNECTED

        num_messages = 10
        messages = [str(i).encode() for i in range(num_messages)]

        # Mock sending of first 2.5 messages, then sock is busy
        msg_size = len(sock._frame_message(messages[0]))
        self.mock_socket.send.side_effect = [
            msg_size,
            msg_size,
            msg_size // 2,
            BlockingIOError(),
        ]

        for msg in messages:
            sock._queue_message(sock._frame_message(msg))

        sock._flush_send_buffer()

        # Send queue should still have messages
        assert len(sock._send_queue) == 7
        # Offset should be at half the message size
        expected_offset = msg_size // 2
        assert sock._offset == expected_offset
        # Sending should be True and the state connected
        assert sock._sending
        assert sock._state == ConnectionState.CONNECTED

    def test_flush_send_buffer_partial_queue_send_with_os_error(self):
        sock = TCPSocket()
        sock._state = ConnectionState.CONNECTED

        num_messages = 10
        messages = [str(i).encode() for i in range(num_messages)]

        # Mock sending of first 2.5 messages, then sock is busy
        msg_size = len(sock._frame_message(messages[0]))
        self.mock_socket.send.side_effect = [
            msg_size,
            msg_size,
            msg_size // 2,
            OSError(),
        ]

        for msg in messages:
            sock._queue_message(sock._frame_message(msg))

        sock._flush_send_buffer()

        # Send queue and recv buffer should be cleared
        assert sock._send_queue == []
        assert sock._recv_buffer == bytearray()
        assert sock._send_buffer == bytearray()
        # Offset should be at 0
        assert sock._offset == 0
        # Sending should be False and the state disconnected
        assert not sock._sending
        assert sock._state == ConnectionState.DISCONNECTED

    def test_recv_all_available(self):
        sock = TCPSocket(self.mock_socket)  # pyright: ignore[reportArgumentType]

        # When recv returns None, socket is closed and None is returned
        self.mock_socket.recv.return_value = None
        sock._recv_all_available()
        assert sock._state == ConnectionState.DISCONNECTED

        # Test two messages coming in
        self.mock_socket.recv.side_effect = [
            b"\x00\x00\x00\x02\x30\x31\x00\x00\x00\x04\x30\x31\x30\x31",
            BlockingIOError(),
        ]
        sock._recv_all_available()
        assert len(sock._recv_buffer) == 14

    def test_grab_all_available_messages(self):
        sock = TCPSocket()
        assert len(sock._recv_buffer) == 0
        assert sock._grab_all_available_messages() is None

        sock._recv_buffer.extend(
            b"\x00\x00\x00\x02\x30\x31\x00\x00\x00\x04\x30\x31\x30\x31"
        )
        assert sock._grab_all_available_messages() == [b"\x30\x31", b"\x30\x31\x30\x31"]
        assert len(sock._recv_buffer) == 0

    def test_grab_next_message_incomplete_header(self):
        sock = TCPSocket()
        sock._recv_buffer.extend(b"\x00\x00")

        assert sock._grab_next_message() is None
        assert len(sock._recv_buffer) == 2

    def test_grab_next_message_complete_header_no_data(self):
        sock = TCPSocket()
        sock._recv_buffer.extend(b"\x00\x00\x00\x01")

        assert sock._grab_next_message() is None
        assert len(sock._recv_buffer) == 4

    def test_grab_next_message_complete_header_partial_data(self):
        sock = TCPSocket()
        sock._recv_buffer.extend(b"\x00\x00\x00\x02\x30")

        assert sock._grab_next_message() is None
        assert len(sock._recv_buffer) == 5

    def test_grab_next_message_complete_header_full_data(self):
        sock = TCPSocket()
        sock._recv_buffer.extend(b"\x00\x00\x00\x02\x30\x31")

        assert sock._grab_next_message() == b"\x30\x31"
        assert len(sock._recv_buffer) == 0

    def test_grab_next_message_size_over_limit(self):
        sock = TCPSocket()
        sock._recv_buffer.extend(b"\x00\x00\x00\x06\x30\x31\x30\x31\x30\x31")
        sock.MAX_MESSAGE_SIZE = 4
        assert sock._grab_next_message() is None
        assert len(sock._recv_buffer) == 0

    def test_grab_next_message_multiple_messages_in_buffer(self):
        sock = TCPSocket()
        sock._recv_buffer.extend(
            b"\x00\x00\x00\x02\x30\x31\x00\x00\x00\x04\x30\x31\x30\x31"
        )
        assert sock._grab_next_message() == b"\x30\x31"
        assert len(sock._recv_buffer) == 8
        assert sock._grab_next_message() == b"\x30\x31\x30\x31"
        assert len(sock._recv_buffer) == 0


# Integration Tests
class MockTCPServer:
    def __init__(self, host="0.0.0.0", port: int = 0, max_conn: int = 1):
        self.host = host
        self.port = port
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((host, port))  # Listen on all interfaces
        self.server_socket.listen(max_conn)
        self.server_socket.setblocking(False)  # Blocking

        self.clients = []

        if port == 0:
            self.port = self.server_socket.getsockname()[1]

    def start(self):
        while True:
            try:
                # Accept pending connections
                try:
                    client_sock, client_addr = self.server_socket.accept()
                    print(f"Connection accepted from {client_addr}")
                    # Wrap in TCPSocket
                    tcp_sock = TCPSocket(client_sock)
                    self.clients.append(tcp_sock)
                except BlockingIOError:
                    pass

                # Receive Data
                for client in self.clients:
                    data = client.receive()
                    # Respond to the client
                    if data:
                        print(f"Server: Data Received from client: {data}")
                        for msg in data:
                            print("Server: Sending data to client.")
                            client.send(b"ACK: " + msg)
                    if client.is_sending():
                        client._flush_send_buffer()
            except BlockingIOError:
                pass
            except OSError:
                break  # Server socket closed

    def stop(self):
        self.server_socket.close()


@pytest.fixture
def tcp_server():
    server = MockTCPServer()

    # Run the server loop in a background thread
    server_thread = threading.Thread(target=server.start, daemon=True)
    server_thread.start()

    yield server

    # Teardown
    server.stop()
    server_thread.join()


def tcp_client(msgs: list[bytes], host: str, port: int, time_between_msgs: float):
    client_sock = TCPSocket()

    print("Connecting to server...")
    success = client_sock.connect(host, port, timeout=5)

    assert success
    print(f"Connection successful: {success}")

    print("Sending Messages...")
    for msg in msgs:
        client_sock.send(msg)
    print("Messages Sent!")

    data = []
    while len(data) < len(msgs):
        new_data = client_sock.receive()
        if not new_data:
            time.sleep(time_between_msgs)
        else:
            data += new_data

    client_sock.close()

    return data


class TestSingleConnection:
    """Test basic one client on server tcp connection."""

    def test_connection_establishment(self, tcp_server):

        client_sock = TCPSocket()

        print("Connecting to server...")
        success = client_sock.connect(tcp_server.host, tcp_server.port, timeout=5)

        assert success

        client_sock.close()

    def test_multiple_messages(self, tcp_server):
        num_messages = 10

        messages = [str(i).encode() for i in range(num_messages)]

        data = tcp_client(messages, tcp_server.host, tcp_server.port, 0.001)

        results = [f"ACK: {str(i)}".encode() for i in range(num_messages)]

        assert len(results) == num_messages
        assert set(data) == set(results)

    def test_rapid_messages_sent(self, tcp_server):
        num_messages = 10

        messages = [str(i).encode() for i in range(num_messages)]

        data = tcp_client(messages, tcp_server.host, tcp_server.port, 0)

        results = [f"ACK: {str(i)}".encode() for i in range(num_messages)]

        assert len(results) == num_messages
        assert set(data) == set(results)

    def test_slow_messages_sent(self, tcp_server):
        num_messages = 10

        messages = [str(i).encode() for i in range(num_messages)]

        data = tcp_client(messages, tcp_server.host, tcp_server.port, 1)

        results = [f"ACK: {str(i)}".encode() for i in range(num_messages)]

        assert len(results) == num_messages
        assert set(data) == set(results)
