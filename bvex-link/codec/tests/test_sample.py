import pytest
from bvex_codec.sample import (
    Sample,
    SampleMetadata,
    WhichDataType,
    PrimitiveData,
    WhichPrimitive,
    FileData,
)


def test_primitive_sample_encode_decode():
    # Test with integer
    metadata = SampleMetadata(
        metric_id="test_metric",
        timestamp=1234567890.0,
        which_data_type=WhichDataType.PRIMITIVE,
    )
    data = PrimitiveData.from_value(42)
    sample = Sample(metadata=metadata, data=data)

    # Encode to JSON
    json_str = sample.model_dump_json()

    # Decode from JSON
    decoded_sample = Sample.model_validate_json(json_str)

    assert decoded_sample.metadata.metric_id == "test_metric"
    assert decoded_sample.metadata.timestamp == 1234567890.0
    assert decoded_sample.metadata.which_data_type == WhichDataType.PRIMITIVE
    if isinstance(decoded_sample.data, PrimitiveData):
        assert decoded_sample.data.which_primitive == WhichPrimitive.INT
        assert decoded_sample.data.value == 42


def test_file_sample_encode_decode():
    # Test with file data
    metadata = SampleMetadata(
        metric_id="test_file",
        timestamp=1234567890.0,
        which_data_type=WhichDataType.FILE,
    )
    data = FileData(filename="test.txt", data=b"Hello, World!")
    sample = Sample(metadata=metadata, data=data)

    # Encode to JSON
    json_str = sample.model_dump_json()

    # Decode from JSON
    decoded_sample = Sample.model_validate_json(json_str)

    assert decoded_sample.metadata.metric_id == "test_file"
    assert decoded_sample.metadata.timestamp == 1234567890.0
    assert decoded_sample.metadata.which_data_type == WhichDataType.FILE
    if isinstance(decoded_sample.data, FileData):
        assert decoded_sample.data.filename == "test.txt"
        assert decoded_sample.data.data == b"Hello, World!"


def test_all_primitive_types():
    # Test all primitive types
    test_cases = [
        (42, WhichPrimitive.INT),
        (3.14, WhichPrimitive.FLOAT),
        ("test string", WhichPrimitive.STR),
        (True, WhichPrimitive.BOOL),
    ]

    for value, expected_type in test_cases:
        metadata = SampleMetadata(
            metric_id=f"test_{expected_type.value}",
            timestamp=1234567890.0,
            which_data_type=WhichDataType.PRIMITIVE,
        )
        data = PrimitiveData.from_value(value)
        sample = Sample(metadata=metadata, data=data)

        json_str = sample.model_dump_json()
        decoded_sample = Sample.model_validate_json(json_str)

        if isinstance(decoded_sample.data, PrimitiveData):
            assert decoded_sample.data.which_primitive == expected_type
            assert decoded_sample.data.value == value


def test_invalid_data_type():
    # Test invalid data type combination
    metadata = SampleMetadata(
        metric_id="test_invalid",
        timestamp=1234567890.0,
        which_data_type=WhichDataType.PRIMITIVE,
    )
    data = FileData(filename="test.txt", data=b"Invalid")

    with pytest.raises(ValueError):
        Sample(metadata=metadata, data=data)


def test_invalid_primitive_type():
    # Test invalid primitive type
    metadata = SampleMetadata(
        metric_id="test_invalid_primitive",
        timestamp=1234567890.0,
        which_data_type=WhichDataType.PRIMITIVE,
    )

    with pytest.raises(ValueError):
        PrimitiveData(which_primitive=WhichPrimitive.INT, value="not an int")
