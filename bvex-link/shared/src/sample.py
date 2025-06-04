from shared.src.encode_telemetry import SegmentationParams, SampleDatagram, SegmentedDataDict


class Sample:
    def __init__(self, metric_id: str, sample_time: int, sample_data: bytes):
        self.metric_id = metric_id
        self.sample_data = sample_data
        self.sample_time = sample_time

    def get_id(self) -> str:
        return self.metric_id + str(self.sample_time)

    def get_data_segment(self, seq_num, segment_size) -> bytes:
        return self.sample_data[
            seq_num * segment_size:
            min(
                len(self.sample_data),
                (seq_num + 1) * segment_size
            )
        ]

    # TODO: change to segmentation_params: SegmentationParameters | None = None
    def get_datagram(self, seq_num, segment_size, num_segments) -> SampleDatagram:
        segmentation_params: SegmentationParams = {
            'seq_num': seq_num,
            'segment_size': segment_size,
            'num_segments': num_segments,
            'segment_data': self.get_data_segment(seq_num, segment_size)
        }
        sample_segment_data_dict: SegmentedDataDict = {
            'metric_id': self.metric_id,
            'sample_time': self.sample_time,
            'sample_data_segment': segmentation_params
        }
        return SampleDatagram(
            sample_segment_data_dict
        )
    
    # TODO: make more robust
    def min_datagram_size(self) -> int:
        segmentation_params: SegmentationParams = {
            'seq_num': 0,
            'segment_size': 1,
            'num_segments': 1,
            'segment_data': "0".encode()
        }
        sample_segment_data_dict: SegmentedDataDict = {
            'metric_id': self.metric_id,
            'sample_time': self.sample_time,
            'sample_data_segment': segmentation_params
        }
        return SampleDatagram(
            sample_segment_data_dict
        ).size()
