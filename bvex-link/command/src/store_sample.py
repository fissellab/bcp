import os
from bvex_codec.sample import Sample, WhichPrimitive, PrimitiveData, FileData
import aiofiles
import numpy as np
import time

SAMPLE_STORE_DIR = os.path.abspath("data")





class SampleStore:
    def __init__(self, sample_store_dir_path: str) -> None:
        if not os.path.exists(sample_store_dir_path):
            os.makedirs(sample_store_dir_path)
        self.sample_store_dir_path = sample_store_dir_path
        self.primitive_files: dict[str, str] = {}

    async def store_sample(self, sample: Sample):
        def store_file(sample: Sample):
            assert isinstance(sample.data, FileData)
            metric_dir = os.path.join(
                SAMPLE_STORE_DIR, f"metric-{sample.metadata.metric_id}")
            if not os.path.exists(metric_dir):
                os.makedirs(metric_dir)
            sample_file = os.path.join(
                metric_dir, f"{sample.data.filename}")
            with open(sample_file, "wb") as f:
                f.write(sample.data.data)

        async def store_primitive(sample: Sample):
            assert isinstance(sample.data, PrimitiveData)
            assert sample.data.which_primitive in [
                WhichPrimitive.FLOAT, WhichPrimitive.BOOL, WhichPrimitive.INT]
            assert isinstance(sample.data.value, float) or isinstance(
                sample.data.value, int) or isinstance(sample.data.value, bool)
            
            filepath = os.path.join(
                    self.sample_store_dir_path, f"metric-{sample.metadata.metric_id}.csv")
            
            if sample.metadata.metric_id not in self.primitive_files:
                self.primitive_files[sample.metadata.metric_id] = filepath
                async with aiofiles.open(filepath, mode='w') as f:
                    await f.write("timestamp,value\n")
            
            async with aiofiles.open(filepath, mode='a') as f:
                await f.write(f"{time.ctime()},{sample.data.value}\n")

        if isinstance(sample.data, FileData):
            store_file(sample)
        elif isinstance(sample.data, PrimitiveData):
            await store_primitive(sample)
        else:
            raise ValueError(f"Unknown data type: {type(sample.data)}")
