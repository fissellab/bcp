import socket
from typing import Callable
import redis
import redis.asyncio
import asyncio
import logging
from bcp_redis_client import SampleSubscriber, get_all_metric_ids, get_sample_async
from bvex_codec.sample import Sample
from bvex_codec.telecommand import CommandTypes, Subscribe, GetMetricIds
from bvex_codec.telemetry import Telemetry, WhichTMMessageType, MetricIds

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)

r = redis.Redis()
r_async = redis.asyncio.Redis()
max_downlink_hz = 2


async def downlink_latest_sample(writer: asyncio.StreamWriter, sample: Sample) -> int:
    telemetry = Telemetry(which_type=WhichTMMessageType.SAMPLE, data=sample)
    data = telemetry.model_dump_json().encode()
    writer.write(data)
    await writer.drain()
    return len(data)


# downlinks samples at approximately get_bps() bitrate
async def downlink_loop(
    writer: asyncio.StreamWriter,
    metric_id: str,
    get_bps: Callable[[], int],
):
    # get socket we are writing to
    sock = writer.get_extra_info("socket")
    sock_buf_size = sock.getsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF)

    # downlink forever
    last_sample = None
    while True:
        sample = await get_sample_async(r_async, metric_id)
        if sample is not None and sample != last_sample:
            last_sample = sample
            bytes_sent = await downlink_latest_sample(writer, sample)

            if (
                sock is not None
                and bytes_sent > 0
                and (sock_buf_size < bytes_sent + 1 or sock_buf_size > bytes_sent * 2)
            ):
                # set socket send buffer to twice the size of the sample
                # to ensure the socket cannot queue many samples in the send buffer
                # (older samples are not useful anymore)
                sock_buf_size = bytes_sent * 2
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, sock_buf_size)

            min_sleep = 1 / max_downlink_hz
            await asyncio.sleep(max(min_sleep, bytes_sent / get_bps()))
        else:
            # sleep for 1 second if no data was sent
            # to avoid busy-waiting on a new sample
            await asyncio.sleep(1)
            continue


async def handle_subscribe(writer: asyncio.StreamWriter, cmd: Subscribe):
    try:
        logger.info(f"downlinking {cmd.metric_id}")
        await downlink_loop(writer, cmd.metric_id, lambda: 10000)
    except asyncio.CancelledError:
        logger.info(f"downlink cancelled for {cmd.metric_id}")
        raise


async def handle_get_metric_ids(writer: asyncio.StreamWriter):
    metric_ids = list(await get_all_metric_ids(r_async))
    telemetry = Telemetry(which_type=WhichTMMessageType.METRIC_IDS, data=MetricIds(metric_ids=metric_ids))
    data = telemetry.model_dump_json().encode()
    writer.write(data)
    await writer.drain()


async def run_command(writer: asyncio.StreamWriter, cmd: CommandTypes) -> None:
    if isinstance(cmd, Subscribe):
        await handle_subscribe(writer, cmd)
    elif isinstance(cmd, GetMetricIds):
        await handle_get_metric_ids(writer)
