"""Test suite for pure Python serialization implementation."""

from dataclasses import dataclass

import msgpack
import numpy as np
import pytest
from pydantic import BaseModel

from skywing.core.registry import DATA_REGISTRY
from skywing.core.serialization import (
    _numpy_decoder,
    _numpy_encoder,
    deserialize,
    serialize,
)
from skywing.core.types import DataMessage, HandshakeMessage, ProcessorData


class TestSerialize:
    """Test serialization."""

    def test_serialize_handshake_message(self):
        hs = HandshakeMessage(remote_id="id", port=90)
        encoded_hs = serialize(hs)
        assert hs != encoded_hs
        assert isinstance(encoded_hs, bytes)

    def test_serialize_data_message(self):
        class TestData(ProcessorData):
            data: str

        d = DataMessage(
            remote_id="id", tag="tag", data=TestData(data="TEST"), version=1
        )
        encoded_d = serialize(d)
        assert d != encoded_d
        assert isinstance(encoded_d, bytes)
        DATA_REGISTRY.reset()

    def test_serialize_invalid_message(self):
        with pytest.raises(AttributeError):
            serialize("TEST")  # type: ignore

    def test_serialize_invalid_data_message(self):
        @dataclass
        class Data:
            data: str

        d = Data(data="test")

        msg = DataMessage(remote_id="id", tag="tag", data=d, version=1)

        with pytest.raises(
            TypeError, match="DataMessage data must be a registered ProcessorData type"
        ):
            serialize(msg)


class TestDeserialize:
    """Test deserialization."""

    def test_deserialize_handshake_message(self):
        hs = HandshakeMessage(remote_id="id", port=90)
        decoded_hs = deserialize(serialize(hs))
        assert hs == decoded_hs
        assert isinstance(decoded_hs, HandshakeMessage)

    def test_deserialize_data_message(self):
        class TestData(ProcessorData):
            data: str

        d = DataMessage(
            remote_id="id", tag="tag", data=TestData(data="TEST"), version=1
        )
        decoded_d = deserialize(serialize(d))
        assert d == decoded_d
        assert isinstance(decoded_d, DataMessage)
        assert decoded_d.data == d.data
        assert isinstance(decoded_d.data, TestData)
        DATA_REGISTRY.reset()

    def test_deserialize_bad_message(self):
        with pytest.raises(ValueError, match="Failed to deserialize message"):
            deserialize(b"abcdefg")

    def test_deserialize_invalid_message(self):
        class TestData(BaseModel):
            data: str

        packed = msgpack.packb(
            TestData(data="TEST").model_dump(mode="python"), use_bin_type=True
        )
        assert packed is not None  # msgpack.packb always returns bytes
        with pytest.raises(ValueError, match="Message does not pass validation"):
            deserialize(packed)

    def test_deserialize_invalid_data_message(self):

        class TestData(ProcessorData):
            data: str

        d = DataMessage(
            remote_id="id", tag="tag", data=TestData(data="TEST"), version=1
        )
        dumped_msg = d.model_dump(mode="python")
        del dumped_msg["data"]["data"]
        dumped_msg["data"]["bad"] = "BAD DATA"
        invalid_msg = msgpack.packb(dumped_msg, use_bin_type=True)
        assert invalid_msg is not None  # msgpack.packb always returns bytes
        with pytest.raises(ValueError, match="Could not validate ProcessorData"):
            deserialize(invalid_msg)
        DATA_REGISTRY.reset()

    def test_deserialize_extra_field_removed(self):
        class TestData(ProcessorData):
            data: str

        d = DataMessage(
            remote_id="id", tag="tag", data=TestData(data="TEST"), version=1
        )
        dumped_msg = d.model_dump(mode="python")
        dumped_msg["data"]["bad_value"] = "TEST"  # insert extra value
        packed = msgpack.packb(dumped_msg, use_bin_type=True)
        assert packed is not None  # msgpack.packb always returns bytes
        decoded_msg = deserialize(packed)
        assert decoded_msg == d
        DATA_REGISTRY.reset()


class TestNumpyEncoder:
    """Test custom numpy array encoder."""

    def test_numpy_encode_1d_numpy_array_float(self):
        numpy_array = np.array([3.3, 4.4, 5.5])
        encoded = _numpy_encoder(numpy_array)
        assert encoded["__numpy__"]
        assert encoded["dtype"] == "float64"
        assert encoded["shape"] == (3,)
        assert encoded["data"] == numpy_array.tobytes()

    def test_numpy_encode_2d_numpy_array_float(self):
        numpy_array = np.array([[1.1, 2.2, 3.3], [3.3, 4.4, 5.5]])
        encoded = _numpy_encoder(numpy_array)
        assert encoded["__numpy__"]
        assert encoded["dtype"] == "float64"
        assert encoded["shape"] == (2, 3)
        assert encoded["data"] == numpy_array.tobytes()

    def test_numpy_encode_1d_numpy_array_int(self):
        numpy_array = np.array([3, 4, 5])
        encoded = _numpy_encoder(numpy_array)
        assert encoded["__numpy__"]
        assert encoded["dtype"] == "int64"
        assert encoded["shape"] == (3,)
        assert encoded["data"] == numpy_array.tobytes()

    def test_numpy_encode_2d_numpy_array_int(self):
        numpy_array = np.array([[1, 2, 3], [3, 4, 5]])
        encoded = _numpy_encoder(numpy_array)
        assert encoded["__numpy__"]
        assert encoded["dtype"] == "int64"
        assert encoded["shape"] == (2, 3)
        assert encoded["data"] == numpy_array.tobytes()

    def test_numpy_encode_non_numpy(self):
        val = [4.4, 5.5, 6.6]
        encoded = _numpy_encoder(val)
        assert encoded == val
        val = "TEST STRING"
        encoded = _numpy_encoder(val)
        assert encoded == val
        val = 1234
        encoded = _numpy_encoder(val)
        assert encoded == val


class TestNumpyDecoder:
    """Test custom numpy array decoder."""

    def test_numpy_decode_1d_numpy_array_float(self):
        numpy_array = np.array([3.3, 4.4, 5.5])
        encoded = {
            "__numpy__": True,
            "shape": (3,),
            "dtype": "float64",
            "data": numpy_array.tobytes(),
        }

        decoded = _numpy_decoder(encoded)
        assert np.array_equal(decoded, numpy_array)

    def test_numpy_decode_2d_numpy_array_float(self):

        numpy_array = np.array([[1.1, 2.2, 3.3], [3.3, 4.4, 5.5]])
        encoded = {
            "__numpy__": True,
            "shape": (2, 3),
            "dtype": "float64",
            "data": numpy_array.tobytes(),
        }

        decoded = _numpy_decoder(encoded)
        assert np.array_equal(decoded, numpy_array)

    def test_numpy_decode_1d_numpy_array_int(self):

        numpy_array = np.array([3, 4, 5])
        encoded = {
            "__numpy__": True,
            "shape": (3,),
            "dtype": "int64",
            "data": numpy_array.tobytes(),
        }

        decoded = _numpy_decoder(encoded)
        assert np.array_equal(decoded, numpy_array)

    def test_numpy_decode_2d_numpy_array_int(self):

        numpy_array = np.array([[1, 2, 3], [3, 4, 5]])
        encoded = {
            "__numpy__": True,
            "shape": (2, 3),
            "dtype": "int64",
            "data": numpy_array.tobytes(),
        }

        decoded = _numpy_decoder(encoded)
        assert np.array_equal(decoded, numpy_array)

    def test_numpy_decode_non_dict(self):
        val = [4.4, 5.5, 6.6]
        decoded = _numpy_decoder(val)  # type: ignore
        assert decoded == val

    def test_numpy_decode_malformed_encoding(self):
        val = {"__numpy__": True}
        decoded = _numpy_decoder(val)
        assert decoded == val
