import os
from bvex_codec.sample import (
    Sample,
    SampleMetadata,
    WhichDataType,
    PrimitiveData,
    WhichPrimitive,
    FileData,
)

c_test_executable_path = os.path.join(
    os.path.dirname(__file__), "..", "build", "bvex_codec_test")

def test_encode_primitive():
    for input_vals in [(WhichPrimitive.INT, 123), (WhichPrimitive.FLOAT, 123.0), (WhichPrimitive.BOOL, 0)]:
        which_primitive, value = input_vals
        metadata = SampleMetadata(
            metric_id="test_metric",
            timestamp=1234567890,
            which_data_type=WhichDataType.PRIMITIVE,
        )
        sample = Sample(
            metadata=metadata,
            data=PrimitiveData.from_value(value)
        )
        sample_encoding = sample.model_dump_json()
        sample_encoding_c = os.popen(f"{c_test_executable_path} {metadata.metric_id} {metadata.timestamp} {sample.data.which_primitive.value} {value}").read()
        sample_decoded_c = Sample.model_validate_json(sample_encoding_c)
        assert sample == sample_decoded_c, f"Encoding mismatch for {which_primitive}"
