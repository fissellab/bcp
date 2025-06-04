"""
BCP Redis Client - A client for handling sample data in Redis with type safety.

This package provides functionality for encoding, decoding, and managing sample data
in Redis with support for primitive types (int, float, str, bytes, bool).

The package is organized into two main modules:
- Synchronous operations (sample.py)
- Asynchronous operations (asyncio.py)
"""

from .sample import (
    set_sample,
    set_sample_primitive,
    set_sample_file,
    set_sample_file_from_bytes,
    get_sample,
    SampleSubscriber,
)

from .asyncio import (
    get_all_metric_ids,
    get_sample as get_sample_async,
)

# Export everything from sample module
sample = {
    "set_sample": set_sample,
    "set_sample_primitive": set_sample_primitive,
    "set_sample_file": set_sample_file,
    "set_sample_file_from_bytes": set_sample_file_from_bytes,
    "get_sample": get_sample,
    "SampleSubscriber": SampleSubscriber,
}

# Export everything from asyncio module
asyncio = {
    "get_all_metric_ids": get_all_metric_ids,
    "get_sample": get_sample_async,
}

__all__ = ["sample", "asyncio"]

__version__ = "0.1.0"
