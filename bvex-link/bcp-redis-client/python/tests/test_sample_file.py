import pytest
import redis
import os
import tempfile
from bcp_redis_client.sample import set_sample_file, set_sample_file_from_bytes, get_sample
from bvex_codec.sample import Sample, WhichDataType, FileData
from typing import cast


# Test fixtures
@pytest.fixture
def redis_client():
    r = redis.Redis(host="localhost", port=6379, db=0)
    yield r
    r.flushdb()  # Clean up after tests


@pytest.fixture
def metric_id():
    return "test_metric"

def test_files(redis_client, metric_id):
    for filename in ["strawberries.png", "test.txt"]:
        file_path = os.path.join(os.path.dirname(__file__), "assets", filename)
        _test_file(redis_client, metric_id, file_path)
        _test_file_from_bytes(redis_client, metric_id, file_path)

def _test_file(redis_client, metric_id, file_path):
    # Set the sample file
    set_sample_file(redis_client, metric_id, file_path)

    # Verify the sample was set in Redis
    sample_key = f"sample-cache:{metric_id}"
    assert redis_client.exists(sample_key)
    sample = get_sample(redis_client, metric_id)
    sample_matches_file(cast(Sample, sample), file_path)

def _test_file_from_bytes(redis_client, metric_id, file_path):
    with open(file_path, "rb") as f:
        file_content = f.read()
    # Set the sample file
    set_sample_file_from_bytes(redis_client, metric_id, file_content, os.path.basename(file_path))

    # Verify the sample was set in Redis
    sample_key = f"sample-cache:{metric_id}"
    assert redis_client.exists(sample_key)
    sample = get_sample(redis_client, metric_id)
    sample_matches_file(cast(Sample, sample), file_path)


def test_set_sample_file_empty(redis_client, metric_id):
    # Create an empty temporary file
    with tempfile.NamedTemporaryFile(delete=False) as f:
        empty_file = f.name

    try:
        set_sample_file(redis_client, metric_id, empty_file)
        sample = get_sample(redis_client, metric_id)
        sample_matches_file(cast(Sample, sample), empty_file)
    finally:
        # Clean up the temporary file
        os.unlink(empty_file)


def test_set_sample_file_nonexistent(redis_client, metric_id):
    # Try to set a non-existent file
    with pytest.raises(FileNotFoundError):
        set_sample_file(redis_client, metric_id, "nonexistent_file.txt")

def file_data_matches_file(file_data: FileData, file_path: str):
    """
    Helper function to check if the file data matches the file on disk.
    """
    with open(file_path, "rb") as f:
        file_content = f.read()
    assert file_data.data == file_content
    assert file_data.filename == os.path.basename(file_path)
    assert len(file_data.data) == os.path.getsize(file_path)

def sample_matches_file(sample: Sample, file_path: str):
    """
    Helper function to check if the encoded sample matches the file on disk.
    """
    assert sample.metadata.which_data_type == WhichDataType.FILE
    assert isinstance(sample.data, FileData)
    file_data_matches_file(sample.data, file_path)