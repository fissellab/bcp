import redis
from bvex_codec.sample import (
    Sample,
    SampleMetadata,
    WhichDataType,
    PrimitiveData,
    FileData,
    PrimitiveType,
)
import time
from threading import Thread, Lock, Condition


def set_sample(r: redis.Redis, metric_id: str, sample: Sample):
    sample_json = sample.model_dump_json()
    r.set(
        f"sample-cache:{sample.metadata.metric_id}",
        sample_json,
    )
    r.publish(f"sample-channel:{sample.metadata.metric_id}", sample_json)


def set_sample_primitive(r: redis.Redis, metric_id: str, value: PrimitiveType):
    """
    Sets the sample primitive in the Redis cache.
    """
    sample = Sample(
        metadata=SampleMetadata(
            metric_id=metric_id,
            timestamp=time.time(),
            which_data_type=WhichDataType.PRIMITIVE,
        ),
        data=PrimitiveData.from_value(value),
    )
    set_sample(r, metric_id, sample)


def set_sample_file(r: redis.Redis, metric_id: str, file_path: str):
    file = FileData.from_file_path(file_path)
    sample = Sample(
        metadata=SampleMetadata(
            metric_id=metric_id,
            timestamp=time.time(),
            which_data_type=WhichDataType.FILE,
        ),
        data=file,
    )
    set_sample(r, metric_id, sample)

def set_sample_file_from_bytes(r: redis.Redis, metric_id: str, data: bytes, filename: str):
    file = FileData(filename=filename, data=data)
    sample = Sample(
        metadata=SampleMetadata(
            metric_id=metric_id,
            timestamp=time.time(),
            which_data_type=WhichDataType.FILE,
        ),
        data=file,
    )
    set_sample(r, metric_id, sample)


def get_sample(r: redis.Redis, metric_id: str) -> Sample | None:
    sample_json = r.get(f"sample-cache:{metric_id}")
    if sample_json is None:
        return None
    return Sample.model_validate_json(sample_json)


class SampleSubscriber:
    """
    Subscribes to the sample channel and keeps track of the latest sample.
    Must be used with a with statement.
    """

    latest_sample: Sample | None

    def __init__(self, r: redis.Redis, metric_id: str):
        self.r = r
        self.metric_id = metric_id
        self.latest_sample_lock = Lock()
        self.latest_sample = None
        self.sample_condition = Condition(self.latest_sample_lock)

    def __enter__(self):
        self.p = self.r.pubsub()
        self.channel = f"sample-channel:{self.metric_id}"
        self.p.subscribe(self.channel)
        self._stop_event = False
        self.thread = Thread(target=self._run)
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._stop_event = True
        self.p.unsubscribe()
        self.thread.join()
        self.p.close()

    def _run(self):
        for message in self.p.listen():
            if self._stop_event:
                break
            if message["type"] == "message":
                sample = Sample.model_validate_json(message["data"])
                assert sample.metadata.metric_id == self.metric_id
                with self.latest_sample_lock:
                    self.latest_sample = sample
                    self.sample_condition.notify_all()

    def get_sample(self) -> Sample | None:
        with self.latest_sample_lock:
            return self.latest_sample

    def get_sample_primitive(self) -> PrimitiveType | None:
        sample = self.get_sample()
        if sample is None or not isinstance(sample.data, PrimitiveData):
            return None
        return sample.data.value

    def wait_for_sample(self, timeout: float | None = None) -> Sample | None:
        """
        Wait for a sample to arrive. If sample is already available, return it immediately.

        Args:
            timeout: Maximum time to wait in seconds. If None, wait indefinitely.

        Returns:
            The new sample if one arrives within the timeout, None otherwise.
        """
        sample = self.get_sample()
        if sample is not None:
            return sample
        with self.sample_condition:
            self.sample_condition.wait(timeout=timeout)
            return self.latest_sample
