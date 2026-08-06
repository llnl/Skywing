"""
Registry system with generation tracking for dynamic type registration.

This module provides a thread-safe registry that tracks when models are registered
and automatically rebuilds TypeAdapters when the registry changes. This is mainly used
for ProcessorData tracking to aide in validation and deserialization.
"""

from typing import Annotated, Optional, Union

from pydantic import BaseModel, Field, TypeAdapter


class TypeRegistry:
    """
    Thread-safe registry with generation tracking.
    """

    def __init__(self, discriminator_field: str):
        """
        Initialize a new registry.

        Args:
            discriminator_field: The field name used to discriminate between types
        """
        self._models: dict[str, type] = {}
        self._generation: int = 0
        self._cached_adapter: Optional[tuple[TypeAdapter[BaseModel], int]] = None
        self._discriminator_field: str = discriminator_field

    def register(self, model_cls: type[BaseModel]) -> None:
        """
        Register a model class.

        Args:
            model_cls: The model class to register

        Returns:
            The same model class (for use as decorator)

        Raises:
            ValueError: If a different class is already registered with the same
                       discriminator value
        """
        # Extract discriminator value from model
        discriminator_value: str = model_cls.model_fields[
            self._discriminator_field
        ].default

        if discriminator_value in self._models:
            # Allow re-registration of same class
            if self._models[discriminator_value] is model_cls:
                return
            # If registered class does not match new class, raise error.
            # This should not be possible since discriminator values are the class names in string form
            raise ValueError(
                f"Duplicate class detected: '{discriminator_value}' has already been registered. "
                + "Update class name to a unique value to register successfully."
            )

        self._models[discriminator_value] = model_cls
        self._generation += 1

    def get_adapter(self) -> TypeAdapter[BaseModel]:
        """
        Get or build TypeAdapter with generation checking.

        Returns a cached TypeAdapter if the registry hasn't changed since it was built.
        Otherwise, builds a new TypeAdapter with the current registered models.

        Returns:
            TypeAdapter for the discriminated union of all registered models

        Raises:
            ValueError: If no models are registered
        """
        # Check if cached adapter is current
        if self._cached_adapter is not None:
            adapter, gen = self._cached_adapter
            if (
                gen == self._generation
            ):  # Make sure cached adapter is most recent adapter
                return adapter

        # Build new adapter if generation mismatch
        if not self._models:
            raise ValueError(
                f"No models registered in {self._discriminator_field} registry. "
                + "Did you forget to import your model classes?"
            )

        model_types = tuple(self._models.values())
        union_type = Union[model_types]
        annotated = Annotated[
            union_type, Field(discriminator=self._discriminator_field)
        ]
        adapter = TypeAdapter(annotated)

        self._cached_adapter = (adapter, self._generation)
        return adapter

    def list_registered(self) -> list[str]:
        """
        List all registered discriminator values.

        Returns:
            List of discriminator values
        """
        return list(self._models.keys())

    def reset(self):
        """Reset registry, clear all models and adapters."""
        self._models = {}
        self._generation = 0
        self._cached_adapter = None


# Global registry for ProcessorData
DATA_REGISTRY = TypeRegistry(discriminator_field="data_type")
