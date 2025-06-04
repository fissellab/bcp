import asyncio
import os
from .cli import CLI
from .subscriber import subscribe_all
from .sync_metric_ids import MetricIdsStore, sync_metric_ids
from bvex_codec.sample import Sample
from .store_sample import SampleStore
from dotenv import load_dotenv


async def run_server():
    remote_ip_addr = os.environ.get("REMOTE_IP_ADDR")
    assert remote_ip_addr is not None, "REMOTE_IP_ADDR environment variable is not set"
    remote_port = os.environ.get("REMOTE_PORT")
    assert remote_port is not None, "REMOTE_PORT environment variable is not set"
    remote_addr: tuple[str, int] = (
        remote_ip_addr,
        int(remote_port)
    )
    metric_ids_store = MetricIdsStore()
    sample_store = SampleStore(os.path.relpath("data"))
    sync_metric_ids_task = asyncio.create_task(
        sync_metric_ids(remote_addr, metric_ids_store))

    async def store_sample(sample: Sample):
        await sample_store.store_sample(sample)
        print(sample)
    subscribe_all_task = asyncio.create_task(
        subscribe_all(remote_addr, metric_ids_store, store_sample))

    await asyncio.gather(sync_metric_ids_task, subscribe_all_task)

