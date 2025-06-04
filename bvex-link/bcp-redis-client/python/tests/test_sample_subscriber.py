import pytest
import redis
import time
from bcp_redis_client.sample import SampleSubscriber, set_sample_primitive
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


# Tests for SampleSubscriber
def test_sample_subscriber_int(redis_client, metric_id):
    # Create subscriber
    with SampleSubscriber(redis_client, metric_id) as subscriber:
        # Set a sample
        value = 42
        set_sample_primitive(redis_client, metric_id, value)

        # Give some time for the subscriber to receive the message
        sample = subscriber.wait_for_sample(timeout=0.5)
        assert sample is not None

        # Get the sample
        assert isinstance(sample, Sample)
        assert sample.metadata.metric_id == metric_id
        assert sample.metadata.which_data_type == WhichDataType.PRIMITIVE
        assert sample.data.value == value
        assert isinstance(sample.data.value, int)


def test_sample_subscriber_float(redis_client, metric_id):
    with SampleSubscriber(redis_client, metric_id) as subscriber:
        value = 3.14
        set_sample_primitive(redis_client, metric_id, value)
        
        sample = subscriber.wait_for_sample(timeout=0.5)
        assert sample is not None

        assert sample.data.value == value
        assert isinstance(sample.data.value, float)


def test_sample_subscriber_str(redis_client, metric_id):
    with SampleSubscriber(redis_client, metric_id) as subscriber:
        value = "test string"
        set_sample_primitive(redis_client, metric_id, value)
        
        sample = subscriber.wait_for_sample(timeout=0.5)
        assert sample is not None

        assert sample.data.value == value
        assert isinstance(sample.data.value, str)


def test_sample_subscriber_bool(redis_client, metric_id):
    with SampleSubscriber(redis_client, metric_id) as subscriber:
        value = True
        set_sample_primitive(redis_client, metric_id, value)

        sample = subscriber.wait_for_sample(timeout=0.5)
        assert sample is not None

        assert sample.data.value == value
        assert isinstance(sample.data.value, bool)


def test_sample_subscriber_no_sample_error(redis_client, metric_id):
    with SampleSubscriber(redis_client, metric_id) as subscriber:
        # Try to get a sample before any have been set
        sample = subscriber.wait_for_sample(timeout=0.5)
        assert sample is None 