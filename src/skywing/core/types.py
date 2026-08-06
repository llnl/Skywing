from typing import Annotated, Any, Literal, Union

from pydantic import BaseModel, ConfigDict, Field, TypeAdapter, ValidationError
from pydantic_core import PydanticCustomError

from .registry import DATA_REGISTRY

# Type aliases
MachineID = str
TagID = str


class HandshakeMessage(BaseModel):
    """Defines the data needed for a handshake message"""

    msg_type: Literal["handshake"] = "handshake"
    remote_id: str
    port: int


class DataMessage(BaseModel):
    """Defines the data needed for a data message"""

    msg_type: Literal["data"] = "data"
    remote_id: str
    tag: str
    data: Any  # type: ignore
    version: int


# Create a global TypeAdapter for all specified message types. Used by serialization.
MessageBase = Union[HandshakeMessage, DataMessage]
MSG_ADAPTER = TypeAdapter(Annotated[MessageBase, Field(discriminator="msg_type")])

# Create backend support for Processer Data class types


class DataMeta(type(BaseModel)):  # type: ignore
    """
    Metaclass that auto-registers ProcessorData subclasses.

    When a class inheriting from ProcessorData is defined, this metaclass
    automatically registers it with DATA_REGISTRY if it has a data_type field.
    """

    @staticmethod
    def __validate_data_type_not_specified(namespace):
        has_explicit_type = "data_type" in namespace
        has_explicit_type_annotation = None
        if "__annotations__" in namespace:
            has_explicit_type_annotation = "data_type" in namespace["__annotations__"]
        elif "__annotate_func__" in namespace:
            annotate_func = namespace["__annotate_func__"]
            annotations = annotate_func(1)  # format=1 returns dict
            has_explicit_type_annotation = "data_type" in annotations

        if has_explicit_type or has_explicit_type_annotation:
            raise ValidationError.from_exception_data(
                title="UserError",
                line_errors=[
                    {
                        "type": PydanticCustomError(
                            "data_type: Literal[str]",
                            "This field is handled internally, manually setting it in your class definition is not allowed.",
                        ),
                        "loc": ("data_type",),
                        "input": "user not allowed to set this member variable",
                    }
                ],
            )

    @staticmethod
    def __add_data_type_and_annotations(name, namespace):
        # In Python 3.13+, annotations may be deferred (PEP 649)
        # Check for both __annotations__ (older) and __annotate_func__ (newer)
        if "__annotate_func__" in namespace:
            original_func = namespace["__annotate_func__"]

            def new_annotate_func(format):  # type: ignore
                annots = original_func(format)
                annots["data_type"] = Literal[name]  # type: ignore
                return annots

            namespace["__annotate_func__"] = new_annotate_func
        else:
            if "__annotations__" not in namespace:
                namespace["__annotations__"] = {}
            namespace["__annotations__"]["data_type"] = Literal[name]  # type: ignore
        namespace["data_type"] = name

    def __new__(mcs, name, bases, namespace, **kwargs):  # type: ignore
        # Only inject data_type for concrete subclasses (not base classes)
        is_concrete_subclass = (
            bases
            and any(b.__name__ == "ProcessorData" for b in bases)
            and not (name == "ProcessorData")
        )
        # If concrete subclass, set the data_type member variable.
        # Overwrite any user inputted value since they should not be setting it
        if is_concrete_subclass:
            mcs.__validate_data_type_not_specified(namespace)

            mcs.__add_data_type_and_annotations(name, namespace)

        cls = super().__new__(mcs, name, bases, namespace, **kwargs)

        if is_concrete_subclass:
            # This is a concrete message class, register it
            DATA_REGISTRY.register(cls)

        return cls


class ProcessorData(BaseModel, metaclass=DataMeta):
    """
    Base class for all Processor data types.

    Subclasses are automatically registered when defined. No decorator needed.
    """

    model_config = ConfigDict(arbitrary_types_allowed=True)
    data_type: Literal["ProcessorData"] = "ProcessorData"
