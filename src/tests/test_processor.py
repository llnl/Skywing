"""Test suite for the base Processor implementation and its subclasses..

Tests the core functionality the base processor and all other child processors.
"""

from collections.abc import Iterable
from itertools import chain
from typing import Any

import numpy as np

from skywing.mid.idempotent_processor import IdemData, IdempotentProcessor


class TestIdempotentProcessor:
    def test_int_data_type(self):
        """Test int data types."""

        data = np.random.randint(1, 100)
        processor = IdempotentProcessor(data, binary_op=max)
        assert processor.query() == max(data, data)

        update_data = np.random.randint(1, 100)
        processor.process_update(
            "tag",
            {"tag": IdemData(global_cnt=processor.global_cnt, global_val=update_data)},
        )

        assert processor.query() == max(data, update_data)

    def test_float_data_type(self):
        """Test float data types."""

        data = np.random.rand()
        processor = IdempotentProcessor(data, binary_op=max)
        assert processor.query() == max(data, data)

        update_data = np.random.rand()
        processor.process_update(
            "tag",
            {"tag": IdemData(global_cnt=processor.global_cnt, global_val=update_data)},
        )

        assert processor.query() == max(data, update_data)

    def test_vector_data_type(self):
        """Test vector data types."""

        def max_func(a: Any, b: Any):
            if isinstance(a, Iterable) and isinstance(b, Iterable):
                ab = [v for v in chain(a, b)]
            elif isinstance(a, Iterable) and not isinstance(b, Iterable):
                ab = chain(a, [b])
            elif isinstance(b, Iterable) and not isinstance(a, Iterable):
                ab = chain(b, [a])
            else:
                ab = [a, b]
            return max(ab)

        data = np.random.randint(1, 10, size=5)
        processor = IdempotentProcessor(data, binary_op=max_func)
        assert processor.query() == max(data)

        update_data = list(np.random.randint(1, 10, size=5))
        processor.process_update(
            "tag",
            {"tag": IdemData(global_cnt=processor.global_cnt, global_val=update_data)},
        )

        assert processor.query() == max(chain(data, update_data))

    def test_mixed_data_type(self):
        """Test mixed data types."""

        def max_func(a: Any, b: Any):
            if isinstance(a, Iterable) and isinstance(b, Iterable):
                ab = [v for v in chain(a, b)]
            elif isinstance(a, Iterable) and not isinstance(b, Iterable):
                ab = chain(a, [b])
            elif isinstance(b, Iterable) and not isinstance(a, Iterable):
                ab = chain(b, [a])
            else:
                ab = [a, b]
            return max(ab)

        data = np.random.rand()
        processor = IdempotentProcessor(data, binary_op=max_func)
        assert processor.query() == max(data, data)

        update_data = np.random.randint(1, 10, size=5)
        processor.process_update(
            "tag",
            {"tag": IdemData(global_cnt=processor.global_cnt, global_val=update_data)},
        )

        assert processor.query() == max(data, max(update_data))
