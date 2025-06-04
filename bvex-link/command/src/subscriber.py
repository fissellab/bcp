import asyncio
from bvex_codec.telecommand import Subscribe, Telecommand
from bvex_codec.telemetry import Telemetry, WhichTMMessageType
from bvex_codec.sample import Sample, WhichDataType, PrimitiveData
from typing import Callable
from .sync_metric_ids import MetricIdsStore
from pydantic import ValidationError
from typing import Awaitable


async def subscribe(
    remote_addr: tuple[str, int], metric_id: str, store_sample: Callable[[Sample], Awaitable[None]]
):
    reader, writer = await asyncio.open_connection(remote_addr[0], remote_addr[1])
    try:
        cmd = Subscribe(metric_id=metric_id)
        tc = Telecommand.from_command(cmd)
        writer.write(tc.model_dump_json().encode())
        await writer.drain()

        while True:
            data = await reader.read(4096)
            if data:
                try:
                    telemetry = Telemetry.model_validate_json(data.decode())
                except ValidationError as e:
                    # packet likely was corrupted
                    print(e)
                    continue
                if isinstance(telemetry.data, Sample):
                    await store_sample(telemetry.data)
            else:
                await asyncio.sleep(0.1)
    except Exception as e:
        print(f"Error: {e}")
        raise e
    finally:
        writer.close()
        await writer.wait_closed()


async def subscribe_all(
    remote_addr: tuple[str, int],
    metric_id_store: MetricIdsStore,
    store_sample: Callable[[Sample], Awaitable[None]],
):
    subscriptions: dict[str, asyncio.Task] = {}

    def get_subscribed_metric_ids():
        return set(subscriptions.keys())

    async def cancel_subscription(metric_id: str):
        if metric_id in subscriptions:
            subscriptions[metric_id].cancel()
            try:
                await subscriptions[metric_id]
            except asyncio.CancelledError:
                pass
            del subscriptions[metric_id]

    async def cancel_subscriptions(metric_ids: set[str]):
        async with asyncio.TaskGroup() as tg:
            for metric_id in metric_ids:
                tg.create_task(cancel_subscription(metric_id))

    async def add_subscription(metric_id: str):
        if metric_id not in subscriptions:
            subscriptions[metric_id] = asyncio.create_task(
                subscribe(remote_addr, metric_id, store_sample)
            )

    async def add_subscriptions(metric_ids: set[str]):
        async with asyncio.TaskGroup() as tg:
            for metric_id in metric_ids:
                tg.create_task(add_subscription(metric_id))

    try:
        while True:
            await metric_id_store.updated.wait()
            new_metric_ids = metric_id_store.get()
            subscribed_metric_ids = get_subscribed_metric_ids()

            # non-subscribed metric ids to subscribe to
            add_metric_ids = new_metric_ids - subscribed_metric_ids
            await add_subscriptions(add_metric_ids)
    finally:
        await cancel_subscriptions(set(subscriptions.keys()))
