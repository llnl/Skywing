from typing import Any

import numpy as np
from pydantic import field_serializer, field_validator

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class ModularCounter:
    """This implements a counter using modular arithmetic.
    This may be incremented infinitely many times, and will
    wrap around to zero when it reaches the MODULUS value.
    Comparison between values respects the wrap around and
    is valid when counters differ by less than HALF."""

    MODULUS = 2**64
    HALF = 2**63

    def __init__(self, value=0):
        self.value = value % self.MODULUS

    def increment(self):
        self.value = (self.value + 1) % self.MODULUS

    def copy(self):
        return ModularCounter(self.value)

    def __int__(self):
        return self.value

    def __repr__(self):
        return f"ModularCounter({self.value})"

    def _diff(self, other):
        return (self.value - other.value) % self.MODULUS

    def __lt__(self, other):
        d = self._diff(other)
        return d > self.HALF

    def __gt__(self, other):
        d = self._diff(other)
        return 0 < d < self.HALF

    def __eq__(self, other):
        return self.value == other.value


class IdemData(ProcessorData):
    global_cnt: ModularCounter
    global_val: Any  # TODO: What is the type of this?

    @field_serializer("global_cnt")
    def serialize_counter(self, cnt: ModularCounter):
        if isinstance(cnt.value, np.float64):
            return float(cnt.value)
        return cnt.value

    @field_validator("global_cnt", mode="before")
    @classmethod
    def validate_counter(cls, v: Any) -> ModularCounter:
        if isinstance(v, ModularCounter):
            return v
        return ModularCounter(value=v)


class IdempotentProcessor(Processor):
    """An idempotent operation is one that can be applied multiple times
    without changing the result. Here, we specfically handle idempotent
    functions with two arguments, that is, functions with the property:

    f(x,y) = f(f(x,y),y) = f(x,f(x,y)) = f(f(x,y),f(x,y))

    This processor can be used to collectively compute operations in
    which the local update has the above property.

    Some examples include maximum, minimum, unions, logical-AND, and
    logical-OR. This does NOT include operations like summation.

    The processor has a versioning feature, so that a user can
    call the ``update_data()`` function to alter the value mid-iteration.
    This allows for older-version values to be discarded to prevent
    stale values from continuing to poison the results if a newer
    result is present.
    """

    def __init__(self, data, **kwargs):
        """Initialize the IdempotentProcessor.

        Args:
            data: Initial data value
            **kwargs: Keyword arguments, must include ``binary_op``
        """
        super().__init__(data, **kwargs)

        # Ensure a valid binary operation is given
        if self.parameters.get("binary_op") is None:
            raise RuntimeError(
                "IdempotentProcessor requires binary_op to be passed as a keyword argument."
            )
        self.binary_op = self.parameters["binary_op"]
        self.test_binary_op()

        # Initialize local and global counters
        self.local_cnt = ModularCounter()
        self.global_cnt = ModularCounter()

        # Initialize global value and result
        self.local_val = self.data
        self.global_val = self.binary_op(self.local_val, self.local_val)
        self.result = self.global_val

    def update_data(self, data):
        """Update the local data.  Note: when updating data, we increment the local counter.

        Args:
            data: The updated data value
        """
        super().update_data(data)
        self.local_val = self.data
        self.local_cnt.increment()

    def process_update(self, my_tag: str, recv_data: dict[str, IdemData]):
        """Apply the binary operation, ``self.binary_op``, to all received
        values, taking versioning into account.

        Args:
           my_tag: The local agent's communication tag
           recv_data: Dictionary with most recent communicated data from neighbors
        """
        for idemdata in recv_data.values():
            nbr_cnt = idemdata.global_cnt
            nbr_val = idemdata.global_val
            # Update the global value if nbr has a newer value
            if nbr_cnt > self.global_cnt:
                self.global_cnt = nbr_cnt.copy()
                self.global_val = nbr_val
            # If nbr has a current value, apply the binary operation
            elif nbr_cnt == self.global_cnt:
                self.global_val = self.binary_op(self.global_val, nbr_val)

        # Update our local counter
        self.local_cnt = max(self.global_cnt, self.local_cnt).copy()

    def compare_global_local(self):
        """Compare the global and local values, taking into account
        whether we have newer local info.
        """

        # Update the global value and counter if our local value is newer
        if self.local_cnt > self.global_cnt:
            self.global_cnt = self.local_cnt.copy()
            self.global_val = self.local_val
        # Otherwise, apply the binary operation to update the global value with our local value
        else:
            self.global_val = self.binary_op(self.global_val, self.local_val)

    def query(self):
        """Return current result stored in ``self.result``. Note: when querying the result,
        ensure that you provide the most up to date information.
        """

        self.compare_global_local()

        # Update and return result
        self.result = self.global_val
        return self.result

    def prepare_for_publication(self):
        """Return current global version counter and value.  Note: when publishing,
        ensure that you provide the most up to date information.
        """

        self.compare_global_local()

        return IdemData(global_cnt=self.global_cnt, global_val=self.global_val)

    def convert(self, publish_data: IdemData):
        # WM: todo - would ultimately like to support general types here... but for now,
        # need to explicitly pack the data type, which means I need to have specific
        # implementation for different types...
        pass
        cnt = publish_data.global_cnt
        val = publish_data.global_val

        if isinstance(val, float):
            val = [val]
        elif isinstance(val, list):
            pass
        elif isinstance(val, np.ndarray):
            val = list(val)
        else:
            raise RuntimeError("Unsupported type for IdempotentProcessor.")

        return [], val, [int(cnt)]

    def deconvert(self, recv_strings, recv_doubles, recv_ints):
        # WM: todo - again, need type-specific implementation

        cnt = ModularCounter(recv_ints[0])
        val = recv_doubles

        if isinstance(self.global_val, float):
            val = val[0]
        elif isinstance(self.global_val, list):
            pass
        elif isinstance(self.global_val, np.ndarray):
            val = np.array(val)
        else:
            raise RuntimeError("Unsupported type for IdempotentProcessor.")

        return IdemData(global_cnt=cnt, global_val=val)

    def test_binary_op(self):
        """Ensure that the binary operation is idempotent."""

        is_idempotent = True

        # Test binary_op(a, a) == binary_op(a, binary_op(a,a))
        a = self.data
        b = self.binary_op(a, a)
        c = self.binary_op(a, b)
        if isinstance(b, np.ndarray):
            if not (b == c).all():
                is_idempotent = False
        elif not b == c:
            is_idempotent = False

        # Test binary_op(a, 0) == binary_op(a, binary_op(a, 0))
        a = self.data
        b = a - a
        c = self.binary_op(a, b)
        d = self.binary_op(a, c)
        if isinstance(c, np.ndarray):
            if not (c == d).all():
                is_idempotent = False
        elif not c == d:
            is_idempotent = False

        if not is_idempotent:
            raise RuntimeError(
                "binary_op passed to IdempotentProcessor is not idempotent."
            )


class MaxProcessor(IdempotentProcessor):
    """This processor calculates the maximum value across agents
    (component-wise maximum for vector/matrix-valued data) and
    accounts for changing local data, discarding old values that
    should no longer contribute to the global max.
    """

    def __init__(self, data, **kwargs):
        """Initialize an IdempotentProcessor with ``np.maximum`` as the binary operation.

        Args:
            data: Initial data value
            **kwargs: Additional keyword arguments
        """
        super().__init__(data, binary_op=np.maximum, **kwargs)


class MinProcessor(IdempotentProcessor):
    """This processor calculates the minimum value across agents
    (component-wise minimum for vector/matrix-valued data) and
    accounts for changing local data, discarding old values that
    should no longer contribute to the global min.
    """

    def __init__(self, data, **kwargs):
        """Initialize an IdempotentProcessor with ``np.minimum`` as the binary operation.

        Args:
            data: Initial data value
            **kwargs: Additional keyword arguments
        """
        super().__init__(data, binary_op=np.minimum, **kwargs)
