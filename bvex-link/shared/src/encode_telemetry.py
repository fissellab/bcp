from typing import TypedDict, Union, cast
import json
import base64
from copy import deepcopy


class SegmentationParams(TypedDict):
    seq_num: int
    segment_size: int
    num_segments: int
    segment_data: bytes


class SingletonDataDict(TypedDict):
    metric_id: str
    sample_time: int
    sample_data: bytes


class SegmentedDataDict(TypedDict):
    metric_id: str
    sample_time: int
    sample_data_segment: SegmentationParams


class SampleDatagram:
    # TODO: change to data: Union[SingletonDataDict, SegmentedDataDict]
    def __init__(self, data: SegmentedDataDict):
        self.data = data

    def get_id(self) -> str:
        return self.data['metric_id'] + str(self.data['sample_time'])

    def size(self):
        return len(str(self))

    def json_dict(self) -> dict:
        data_dict = cast(dict, deepcopy(self.data))
        data_dict['sample_data_segment']['segment_data'] = base64.b64encode(
            data_dict['sample_data_segment']['segment_data']
        ).decode("ascii")
        return data_dict

    def __repr__(self):
        return json.dumps(self.json_dict())


class TelemetryPayload:
    def __init__(self, max_size: int):
        self.samples: list[SampleDatagram] = []
        self.max_size = max_size

    def add(self, sample: SampleDatagram):
        # TODO: Make this more robust
        if self.size() + sample.size() + len(", ") > self.max_size:
            raise BufferError("Payload is full")
        self.samples.append(sample)

    def to_bytes(self) -> bytes:
        return json.dumps(
            [sample.json_dict() for sample in self.samples]
        ).encode()

    def size(self):
        return len(self.to_bytes())
