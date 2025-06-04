from shared.src.encode_telemetry import SampleDatagram, SegmentedDataDict
import json
from typing import cast
import base64


def decode_telemetry(payload: bytes) -> list[SampleDatagram]:
    telemetry_payload_json = json.loads(payload)
    sample_datagrams = []
    for sample in telemetry_payload_json:
        sample['sample_data_segment']['segment_data'] = base64.b64decode(
            sample['sample_data_segment']['segment_data'])
        sample_datagrams.append(SampleDatagram(
            cast(SegmentedDataDict, sample)))
    return sample_datagrams
