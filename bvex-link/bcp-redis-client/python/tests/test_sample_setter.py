import pytest
import redis
from bcp_redis_client.sample import set_sample_primitive
from bvex_codec.sample import Sample, WhichDataType


# Test fixtures
@pytest.fixture
def redis_client():
    r = redis.Redis(host="localhost", port=6379, db=0)
    yield r
    r.flushdb()  # Clean up after tests


@pytest.fixture
def metric_id():
    return "test_metric"


# Tests for set_sample_primitive
def test_set_sample_primitive_int(redis_client, metric_id):
    value = 42
    set_sample_primitive(redis_client, metric_id, value)

    # Verify the sample was set in Redis
    sample_key = f"sample-cache:{metric_id}"
    assert redis_client.exists(sample_key)

    # Get the sample from Redis
    sample_data = redis_client.get(sample_key)
    assert sample_data is not None  # Should have data
    sample = Sample.model_validate_json(sample_data)
    assert sample.metadata.metric_id == metric_id
    assert sample.metadata.which_data_type == WhichDataType.PRIMITIVE
    assert sample.data.value == value


def test_set_sample_primitive_float(redis_client, metric_id):
    value = 3.14
    set_sample_primitive(redis_client, metric_id, value)
    sample_key = f"sample-cache:{metric_id}"
    assert redis_client.exists(sample_key)
    sample = Sample.model_validate_json(redis_client.get(sample_key))
    assert sample.data.value == value


def test_set_sample_primitive_str(redis_client, metric_id):
    value = "test string"
    set_sample_primitive(redis_client, metric_id, value)
    sample_key = f"sample-cache:{metric_id}"
    assert redis_client.exists(sample_key)
    sample = Sample.model_validate_json(redis_client.get(sample_key))
    assert sample.data.value == value


def test_set_sample_primitive_bool(redis_client, metric_id):
    value = True
    set_sample_primitive(redis_client, metric_id, value)
    sample_key = f"sample-cache:{metric_id}"
    assert redis_client.exists(sample_key)
    sample = Sample.model_validate_json(redis_client.get(sample_key))
    assert sample.data.value == value 