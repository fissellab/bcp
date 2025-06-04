import asyncio
from bvex_codec.telecommand import GetMetricIds, Telecommand
from bvex_codec.telemetry import Telemetry, WhichTMMessageType, MetricIds
from bvex_codec.sample import Sample, WhichDataType, PrimitiveData
from typing import Callable


class MetricIdsStore:
    def __init__(self):
        self.metric_ids = set()
        self.updated = asyncio.Event()

    # if new metric ids are different from current, 
    # updates metric ids and sets updated flag
    def update(self, new_metric_ids: list[str]):
        if self.metric_ids != set(new_metric_ids):
            self.metric_ids.clear()
            self.metric_ids.update(new_metric_ids)
            self.updated.set()

    # returns set of current metric ids, clears updated flag
    def get(self) -> set[str]:
        self.updated.clear()
        return self.metric_ids.copy()


async def sync_metric_ids(remote_addr: tuple[str, int], metric_ids: MetricIdsStore):
    reader, writer = await asyncio.open_connection(remote_addr[0], remote_addr[1])
    cmd = GetMetricIds()
    tc = Telecommand.from_command(cmd)
    writer.write(tc.model_dump_json().encode())
    await writer.drain()
    while True:
        try:
            async with asyncio.timeout(10.0):
                data = await reader.read(4096)
                if data:
                    telemetry = Telemetry.model_validate_json(data.decode())
                    if isinstance(telemetry.data, MetricIds):
                        metric_ids.update(telemetry.data.metric_ids)
        except asyncio.TimeoutError:
            pass
        await asyncio.sleep(10.0)
