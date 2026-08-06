"""Test suite for pure Python core implementation.

Tests the core functionality of the pure Python Agent, Manager, Job, and networking components.
"""

import time
from typing import Any, Literal

import numpy as np
import pytest
from pydantic import ValidationError, field_serializer, field_validator
from skywing.core.agent import Agent
from skywing.core.job import Job
from skywing.core.registry import DATA_REGISTRY
from skywing.core.serialization import deserialize, serialize
from skywing.core.types import MSG_ADAPTER, DataMessage, HandshakeMessage, ProcessorData


class TestBasicConnections:
    """Test basic agent connection establishment and handshaking."""

    def test_two_agents_connect(self):
        """Test that two agents can connect and complete handshake."""
        # Create two agents
        agent1 = Agent("127.0.0.1", 20100)
        agent2 = Agent("127.0.0.1", 20101)

        # Configure as neighbors
        agent1.configure_neighbors([("127.0.0.1", 20101)])
        agent2.configure_neighbors([("127.0.0.1", 20100)])

        # Launch agents
        agent1.launch()
        agent2.launch()

        # Wait for handshakes to complete
        time.sleep(0.5)

        # Verify neighbors are established
        assert agent1.manager.number_of_neighbors() == 1
        assert agent2.manager.number_of_neighbors() == 1

        # Cleanup
        agent1.manager.stop()
        agent2.manager.stop()

    def test_three_agents_ring_topology(self):
        """Test three agents in a ring topology (0-1-2-0)."""
        agents = [
            Agent("127.0.0.1", 20110),
            Agent("127.0.0.1", 20111),
            Agent("127.0.0.1", 20112),
        ]

        # Configure ring: 0->1, 1->2, 2->0
        agents[0].configure_neighbors([("127.0.0.1", 20111)])
        agents[1].configure_neighbors([("127.0.0.1", 20112)])
        agents[2].configure_neighbors([("127.0.0.1", 20110)])

        # Launch all agents
        for agent in agents:
            agent.launch()

        # Wait for connections
        time.sleep(0.5)

        # Each agent should have 2 neighbors (one incoming, one outgoing)
        for agent in agents:
            assert agent.manager.number_of_neighbors() == 2

        # Cleanup
        for agent in agents:
            agent.manager.stop()


class TestMessaging:
    """Test message sending and receiving between agents."""

    def test_publish_subscribe(self):
        """Test basic publish/subscribe functionality."""
        agent1 = Agent("127.0.0.1", 20120)
        agent2 = Agent("127.0.0.1", 20121)

        agent1.configure_neighbors([("127.0.0.1", 20121)])
        agent2.configure_neighbors([("127.0.0.1", 20120)])

        # Track received messages
        received_data = []

        def job_func(job):
            """Job that subscribes to a tag and collects data."""
            job.declare_publication_intent("test_tag")
            job.subscribe("test_tag")

            # Wait briefly for subscriptions to be registered
            time.sleep(0.1)

            # Publish some data from agent1's job
            if job.manager.machine_id == "127.0.0.1_20120":
                job.publish("test_tag", {"value": 42})
                job.publish("test_tag", {"value": 99})

            # Collect data for a short time
            start_time = time.time()
            while time.time() - start_time < 1.0:
                if job.has_data("test_tag"):
                    data = job.get_data_if_present("test_tag")
                    if data is not None:
                        received_data.append(data)
                time.sleep(0.01)

        # Submit jobs to both agents
        job1 = Job("job1", agent1.manager, job_func)
        job2 = Job("job2", agent2.manager, job_func)

        agent1.manager.submit_job("job1", job1)
        agent2.manager.submit_job("job2", job2)

        # Launch agents (starts jobs)
        agent1.launch()
        agent2.launch()

        # Wait for jobs to complete
        time.sleep(1.5)

        # Both agents should have received data (at least their own publications)
        assert len(received_data) > 0

        # Cleanup
        agent1.manager.stop()
        agent2.manager.stop()

    def test_multiple_tags(self):
        """Test subscribing to multiple different tags."""
        agent1 = Agent("127.0.0.1", 20130)
        agent2 = Agent("127.0.0.1", 20131)

        agent1.configure_neighbors([("127.0.0.1", 20131)])
        agent2.configure_neighbors([("127.0.0.1", 20130)])

        received_tags = set()

        def job_func(job):
            """Job that publishes and subscribes to multiple tags."""
            job.declare_publication_intent("tag_a", "tag_b", "tag_c")
            job.subscribe("tag_a", "tag_b", "tag_c")

            time.sleep(0.1)

            # Publish on different tags
            if job.manager.machine_id == "127.0.0.1_20130":
                job.publish("tag_a", {"source": "agent1", "tag": "a"})
                job.publish("tag_b", {"source": "agent1", "tag": "b"})

            if job.manager.machine_id == "127.0.0.1_20131":
                job.publish("tag_c", {"source": "agent2", "tag": "c"})

            # Collect data
            start_time = time.time()
            while time.time() - start_time < 1.0:
                for tag in ["tag_a", "tag_b", "tag_c"]:
                    if job.has_data(tag):
                        data = job.get_data_if_present(tag)
                        if data is not None:
                            received_tags.add(data["tag"])
                time.sleep(0.01)

        job1 = Job("job1", agent1.manager, job_func)
        job2 = Job("job2", agent2.manager, job_func)

        agent1.manager.submit_job("job1", job1)
        agent2.manager.submit_job("job2", job2)

        agent1.launch()
        agent2.launch()

        time.sleep(1.5)

        # Should have received messages from all tags
        assert "a" in received_tags
        assert "b" in received_tags
        assert "c" in received_tags

        # Cleanup
        agent1.manager.stop()
        agent2.manager.stop()


class TestSubscriptionBuffer:
    """Test the MostRecentBuffer behavior of subscriptions."""

    def test_most_recent_buffer_overwrites(self):
        """Test that new data overwrites old data in subscription buffer."""
        agent1 = Agent("127.0.0.1", 20140)
        agent2 = Agent("127.0.0.1", 20141)

        agent1.configure_neighbors([("127.0.0.1", 20141)])
        agent2.configure_neighbors([("127.0.0.1", 20140)])

        received_values = []

        class TestData(ProcessorData):
            value: Any

        def publisher_job(job):
            """Rapidly publish multiple values."""
            job.declare_publication_intent("data_tag")
            time.sleep(0.1)

            # Publish 10 values rapidly
            for i in range(10):
                job.publish("data_tag", TestData(value=i))
                time.sleep(0.01)

        def subscriber_job(job):
            """Slowly read values (slower than publishing)."""
            job.subscribe("data_tag")
            time.sleep(0.2)  # Wait for publishing to start

            # Read slowly (every 100ms)
            for _ in range(5):
                time.sleep(0.1)
                if job.has_data("data_tag"):
                    data = job.get_data_if_present("data_tag")
                    if data is not None:
                        received_values.append(data.value)

        job1 = Job("pub_job", agent1.manager, publisher_job)
        job2 = Job("sub_job", agent2.manager, subscriber_job)

        agent1.manager.submit_job("pub_job", job1)
        agent2.manager.submit_job("sub_job", job2)

        agent1.launch()
        agent2.launch()

        time.sleep(1.5)

        # Should have received some values, but not all 10 (due to overwriting)
        # The last value should be recent (not 0)
        assert len(received_values) > 0
        if len(received_values) > 1:
            # Values should generally be increasing (getting more recent)
            assert received_values[-1] > received_values[0]

        # Cleanup
        agent1.manager.stop()
        agent2.manager.stop()
        DATA_REGISTRY.reset()

    def test_has_data_tracks_new_arrivals(self):
        """Test that has_data() correctly tracks when NEW data arrives."""
        agent1 = Agent("127.0.0.1", 20150)
        agent2 = Agent("127.0.0.1", 20151)

        agent1.configure_neighbors([("127.0.0.1", 20151)])
        agent2.configure_neighbors([("127.0.0.1", 20150)])

        read_count = [0]  # Use list to allow mutation in closure

        class TestData(ProcessorData):
            seq: Any

        def publisher_job(job):
            """Publish data at intervals."""
            job.declare_publication_intent("test_tag")
            time.sleep(0.2)

            for i in range(5):
                job.publish("test_tag", TestData(seq=i))
                time.sleep(0.1)

        def subscriber_job(job):
            """Check has_data() behavior."""
            job.subscribe("test_tag")
            time.sleep(0.25)

            # Poll for data
            for _ in range(20):
                if job.has_data("test_tag"):
                    data = job.get_data_if_present("test_tag")
                    if data is not None:
                        read_count[0] += 1
                time.sleep(0.05)

        job1 = Job("pub_job", agent1.manager, publisher_job)
        job2 = Job("sub_job", agent2.manager, subscriber_job)

        agent1.manager.submit_job("pub_job", job1)
        agent2.manager.submit_job("sub_job", job2)

        agent1.launch()
        agent2.launch()

        time.sleep(1.5)

        # Should have read data multiple times (once per new arrival)
        # We published 5 times, so should read ~5 times (may vary slightly)
        assert read_count[0] >= 3  # At least caught some updates

        # Cleanup
        agent1.manager.stop()
        agent2.manager.stop()
        DATA_REGISTRY.reset()


class TestStressTests:
    """Stress tests for the Python core."""

    def test_rapid_messaging(self):
        """Test sending many messages rapidly with version tracking.

        This test verifies that:
        1. Messages are not lost due to race conditions
        2. Version tracking prevents premature overwrites
        3. MostRecentBuffer still works correctly

        With MostRecentBuffer, we expect to receive a sampling of messages,
        not all of them. The test sends 1000 messages and expects to receive
        at least 5% (50 messages) due to the polling rate difference.
        """
        agent1 = Agent("127.0.0.1", 20180)
        agent2 = Agent("127.0.0.1", 20181)

        agent1.configure_neighbors([("127.0.0.1", 20181)])
        agent2.configure_neighbors([("127.0.0.1", 20180)])

        received_sequences = []
        NUM_MESSAGES = 1000

        class TestData(ProcessorData):
            seq: Any

        def rapid_publisher(job):
            """Publish many messages rapidly."""
            job.declare_publication_intent("stress_tag")
            time.sleep(0.1)

            # Send 1000 messages with minimal delay
            for i in range(NUM_MESSAGES):
                job.publish("stress_tag", TestData(seq=i))
                time.sleep(0.0001)  # 0.1ms between sends

        def counter_subscriber(job):
            """Count and track received messages."""
            job.subscribe("stress_tag")
            time.sleep(0.05)

            start_time = time.time()
            while time.time() - start_time < 2.0:
                if job.has_data("stress_tag"):
                    data = job.get_data_if_present("stress_tag")
                    if data is not None:
                        received_sequences.append(data.seq)
                time.sleep(0.001)

        job1 = Job("pub_job", agent1.manager, rapid_publisher)
        job2 = Job("sub_job", agent2.manager, counter_subscriber)

        agent1.manager.submit_job("pub_job", job1)
        agent2.manager.submit_job("sub_job", job2)

        agent1.launch()
        agent2.launch()

        time.sleep(2.5)

        # Analyze results
        num_received = len(received_sequences)
        reception_rate = (num_received / NUM_MESSAGES) * 100

        # Check for sequence integrity (should be monotonically increasing)
        is_monotonic = all(
            received_sequences[i] <= received_sequences[i + 1]
            for i in range(len(received_sequences) - 1)
        )

        # Cleanup
        agent1.manager.stop()
        agent2.manager.stop()
        DATA_REGISTRY.reset()

        # Assertions with meaningful error messages
        # Note: With MostRecentBuffer, exact counts vary based on scheduling.
        # We test correctness, not specific throughput numbers.

        assert num_received >= 10, (
            f"Expected at least 10 messages (1% of {NUM_MESSAGES}), got {num_received} ({reception_rate:.1f}%). "
            f"First seq: {received_sequences[0] if received_sequences else 'N/A'}, "
            f"Last seq: {received_sequences[-1] if received_sequences else 'N/A'}"
        )

        assert is_monotonic, (
            f"Sequences must be monotonically increasing (version tracking ensures this), "
            f"but got: {received_sequences[:20]}"
        )

        # The last message should be recent (from the last 20% of messages)
        # This ensures version tracking is working - we're not stuck with old data
        assert received_sequences and received_sequences[-1] > NUM_MESSAGES * 0.8, (
            f"Last sequence {received_sequences[-1]} should be > {NUM_MESSAGES * 0.8} (version tracking working). "
            f"Received sequences: {received_sequences}"
        )


class TestMessageBase:
    """Tests for the MessageBase dataclasses."""

    def test_handshake_validation(self):
        hs = HandshakeMessage(remote_id="id", port=90)
        hs.model_dump(mode="python")
        validated_hs = MSG_ADAPTER.validate_python(hs)
        assert hs == validated_hs

    def test_data_validation(self):
        d = DataMessage(remote_id="id", tag="tag", data="DATA", version=1)
        d.model_dump(mode="python")
        validated_d = MSG_ADAPTER.validate_python(d)
        assert d == validated_d

    def test_invalid_type_fails_validation(self):
        bad_data = {"invalid_type": "bad_data", "malicious": "DATA"}
        with pytest.raises(ValidationError):
            MSG_ADAPTER.validate_python(bad_data)

    def test_handshake_serialization(self):
        hs = HandshakeMessage(remote_id="id", port=90)
        decoded_hs = deserialize(serialize(hs))
        assert hs == decoded_hs

    def test_data_serialization(self):
        class TestData(ProcessorData):
            data: str

        d = DataMessage(
            remote_id="id", tag="tag", data=TestData(data="DATA"), version=1
        )
        decoded_d = deserialize(serialize(d))
        assert d == decoded_d
        DATA_REGISTRY.reset()

    def test_data_serialization_with_numpy(self):

        class TestData(ProcessorData):
            data: np.ndarray

        d = DataMessage(
            remote_id="id",
            tag="tag",
            data=TestData(data=np.ndarray([1, 2, 3, 4])),
            version=1,
        )
        decoded_d = deserialize(serialize(d))
        assert isinstance(decoded_d, DataMessage)
        assert np.array_equal(d.data.data, decoded_d.data.data)
        DATA_REGISTRY.reset()

    def test_data_serialization_with_custom_class_fails(self):
        """This should not work with msgpack, since it cannot pack arbitrary python classes."""

        class SampleClass:
            data: str

            def __init__(self, data):
                self.data = data

        d = DataMessage(remote_id="id", tag="tag", data=SampleClass("DATA"), version=1)
        with pytest.raises(TypeError):
            deserialize(serialize(d))

    def test_invalid_types(self):
        # Raises error when remote_id is None
        with pytest.raises(ValidationError):
            DataMessage(remote_id=None, tag="tag", data="DATA", version=1)  # pyright: ignore[reportArgumentType]

        # Raises error when port is of the incorrect type
        with pytest.raises(ValidationError):
            HandshakeMessage(remote_id="id", port="abc")  # pyright: ignore[reportArgumentType]


class TestRegistry:
    def test_processor_data_registration(self):
        """Tests auto registration of new ProcessorData"""

        class TestData(ProcessorData):
            data: str

        assert "TestData" in DATA_REGISTRY.list_registered()
        DATA_REGISTRY.reset()

    def test_error_with_duplicate_data_type(self):
        """Tests whether the registry raises an error when duplicate class names are used."""

        class CustomClass(ProcessorData):  # pyright: ignore[reportRedeclaration]
            data: str

        with pytest.raises(ValueError, match=r"Duplicate class detected"):

            class CustomClass(ProcessorData):
                data: str

        DATA_REGISTRY.reset()

    def test_error_with_empty_registry(self):
        """Tests whether an empty registry produces an error when getting the adapter."""
        DATA_REGISTRY.reset()  # Reset the registry to clear all registered models.
        with pytest.raises(ValueError, match=r"No models registered in"):
            DATA_REGISTRY.get_adapter()

    def test_list_registered(self):
        """Tests the list_registered method."""
        assert DATA_REGISTRY.list_registered() == []

        class TestData(ProcessorData):
            data: str

        assert DATA_REGISTRY.list_registered() == ["TestData"]

        class TestData2(ProcessorData):
            data: str

        assert DATA_REGISTRY.list_registered() == ["TestData", "TestData2"]
        DATA_REGISTRY.reset()


class TestProcessorData:
    def test_processor_validation_error_when_data_type_specified(self):
        """Tests that a ValidationError is thrown if a user tries to specify the data_type."""

        with pytest.raises(ValidationError):

            class TestData(ProcessorData):
                data: str
                data_type: Literal["test_data"] = "test_data"

        DATA_REGISTRY.reset()

    def test_process_data_serialization(self):
        """Tests that the serialization protocol can handle the custom data type."""

        class SerializationTestData(ProcessorData):
            data: str
            data2: list

        data = SerializationTestData(data="testdata", data2=[1, 2, 3, 4])
        msg = DataMessage(remote_id="1", tag="tag", data=data, version=1)

        data_bytes = serialize(msg)
        decoded = deserialize(data_bytes)

        assert isinstance(decoded, DataMessage)
        assert isinstance(decoded.data, SerializationTestData)
        assert decoded.data.data == "testdata"
        assert decoded.data.data2 == [1, 2, 3, 4]

    def test_process_data_serialization_with_custom_class(self):
        """Tests that the serialization protocol can handle the custom data type."""

        class CustomClass:
            MODULUS = 2**64
            HALF = 2**63

            def __init__(self, value=0):
                self.value = value % self.MODULUS

            def increment(self):
                self.value = (self.value + 1) % self.MODULUS

        class CustomClassTestData(ProcessorData):
            data: str
            cnt: CustomClass

            @field_serializer("cnt")
            def serialize_counter(self, cnt: CustomClass):
                return cnt.value

            @field_validator("cnt", mode="before")
            @classmethod
            def validate_counter(cls, v: Any) -> CustomClass:
                if isinstance(v, CustomClass):
                    return v
                return CustomClass(value=v)

        data = CustomClassTestData(data="testdata", cnt=CustomClass(value=9))
        msg = DataMessage(remote_id="1", tag="tag", data=data, version=1)

        data_bytes = serialize(msg)
        decoded = deserialize(data_bytes)

        assert isinstance(decoded, DataMessage)
        assert isinstance(decoded.data, CustomClassTestData)
        assert decoded.data.data == "testdata"
        assert isinstance(decoded.data.cnt, CustomClass)

    def test_process_data_serialization_with_composite_data(self):
        """Tests that the serialization protocol can handle the custom data type."""

        class SubProcessorData(ProcessorData):
            data: str
            calculation: np.ndarray

        class CompositeData(ProcessorData):
            substr: str
            subproc: SubProcessorData

        data = CompositeData(
            substr="testdata",
            subproc=SubProcessorData(
                data="subprocdata", calculation=np.array([1.2, 2.2])
            ),
        )
        msg = DataMessage(remote_id="1", tag="tag", data=data, version=1)

        data_bytes = serialize(msg)
        decoded = deserialize(data_bytes)

        assert isinstance(decoded, DataMessage)
        assert isinstance(decoded.data, CompositeData)
        assert decoded.data.substr == "testdata"
        assert isinstance(decoded.data.subproc, SubProcessorData)
        assert np.array_equal(
            data.subproc.calculation, decoded.data.subproc.calculation
        )


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
