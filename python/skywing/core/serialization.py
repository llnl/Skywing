from typing import Any

import msgpack
import numpy as np
from loguru import logger
from pydantic import ValidationError

from .registry import DATA_REGISTRY
from .types import MSG_ADAPTER, DataMessage, MessageBase, ProcessorData


def _numpy_encoder(obj: Any) -> Any:
    """
    Custom encoder for numpy arrays in msgpack.

    Encodes numpy arrays as a dict with metadata (dtype, shape) and raw bytes.

    Args:
        obj: Object to encode

    Returns:
        Encoded representation or obj unchanged if not a numpy array
    """
    if isinstance(obj, np.ndarray):
        return {
            "__numpy__": True,
            "dtype": str(obj.dtype),
            "shape": obj.shape,
            "data": obj.tobytes(),
        }
    return obj


def _numpy_decoder(obj: dict[str, Any]) -> Any:
    """
    Custom decoder for numpy arrays in msgpack.

    Decodes numpy arrays from the dict representation created by _numpy_encoder.

    Args:
        obj: Object to decode

    Returns:
        Reconstructed numpy array or obj unchanged if not a numpy array encoding
    """
    if isinstance(obj, dict) and obj.get("__numpy__"):
        fields_exist = [val in obj for val in ["data", "dtype", "shape"]]
        if all(fields_exist):
            return np.frombuffer(obj["data"], dtype=np.dtype(obj["dtype"])).reshape(
                obj["shape"]
            )
        else:
            logger.warning(
                "Malformed numpy encoding detected - obj marked as numpy, but did not contain all required fields ['dtype','data','shape']"
            )
    return obj


def serialize(msg: MessageBase) -> bytes:
    """Serialize a message to bytes.

    Takes the pydantic MessageBase model and serializes. If message is type Data, the data model is dumped as well.

    Args:
        msg: MessageBase

    Returns:
        serialized bytes
    """
    # Make a copy to avoid modifying the original
    msg_copy = msg.model_copy()

    if isinstance(msg_copy, DataMessage):
        data = msg_copy.data
        if isinstance(data, ProcessorData):
            dumped_data = data.model_dump(mode="python")
            msg_copy.data = dumped_data
        else:
            raise TypeError(
                f"DataMessage data must be a registered ProcessorData type, not type {type(data)}."
            )

    try:
        result = msgpack.packb(
            msg_copy.model_dump(mode="python"),
            default=_numpy_encoder,
            use_bin_type=True,
        )
        # msgpack.packb always returns bytes, but type checker doesn't know that
        assert result is not None
        return result
    except TypeError as e:
        logger.error(f"Message could not be serialized - {e}")
        raise e


def deserialize(data: bytes) -> MessageBase:
    """Deserialize bytes to a message dictionary.

    Args:
        data: bytes

    Returns:
        MessageBase instance

    Raises:
        ValueError: If data fails deserialization or validation
    """
    # Unpack the message, catch common msgpack.unpackb errors
    # Validate and catch any validation errors
    try:
        msg_dict = msgpack.unpackb(data, object_hook=_numpy_decoder, raw=False)
        msg = MSG_ADAPTER.validate_python(msg_dict)

        # If data exists, then validate data.
        if isinstance(msg, DataMessage):
            try:
                validated_data = DATA_REGISTRY.get_adapter().validate_python(msg.data)
                msg.data = validated_data
                return msg  # Return DataMessage only if it passes validation
            except ValidationError as e:
                raise ValueError(f"Could not validate ProcessorData: {e}") from e
            except ValueError as e:
                raise ValueError(f"Error occured during validation: {e}") from e
        else:
            return msg  # Return non-DataMessage as is

    except (msgpack.ExtraData, msgpack.FormatError, UnicodeDecodeError) as e:
        raise ValueError(f"Failed to deserialize message: {e}") from e
    except ValidationError as e:
        raise ValueError(f"Message does not pass validation: {e}") from e
