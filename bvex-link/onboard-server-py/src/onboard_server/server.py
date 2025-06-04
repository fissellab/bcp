import asyncio
import socket
import logging
from bvex_codec.telecommand import Telecommand, Cancel, End, CommandTypes
from bvex_codec.telemetry import Telemetry, WhichTMMessageType
from pydantic import ValidationError
import asyncio
import queue
from .handle_commands import run_command

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)

port = 8888


async def handle_connection(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    # Log client connection details
    client_addr = writer.get_extra_info("peername")
    client_str = f"{client_addr[0]}:{client_addr[1]}"
    logger.info(f"New connection from {client_str}")

    def decode_telecommand(data: bytes) -> Telecommand | None:
        if not data:
            return None
        message = data.decode()
        try:
            tc = Telecommand.model_validate_json(message)
            return tc
        except ValidationError as e:
            logger.error(f"Error decoding telecommand: {e}")
            return None

    current_task = None

    async def cancel_current_task():
        if current_task is not None:
            if not current_task.done():
                current_task.cancel()
            try:
                await current_task
            except asyncio.CancelledError:
                pass
            except ConnectionError:
                pass

    command_queue = queue.Queue()
    try:
        # loop through messages until reader is closed
        while True:
            # reader.read() does not block until data is available, immediately returns
            # if no data is available, returns empty bytes
            try:
                data = await reader.read(4096)
            except ConnectionError:
                logger.info(f"Client {client_str} disconnected")
                return
            tc = decode_telecommand(data)
            if tc is not None:
                logger.info(f"Received telecommand of type: {tc.which_command}")
                if isinstance(tc, Cancel):
                    await cancel_current_task()
                    command_queue.queue.clear()
                elif isinstance(tc, End):
                    return
                else:
                    if current_task is None:
                        current_task = asyncio.create_task(run_command(writer, tc.data))
                    else:
                        command_queue.put(tc)
            await asyncio.sleep(0.2)  # sleep to avoid busy-waiting
    except asyncio.CancelledError:
        raise
    finally:
        await cancel_current_task()
        if not writer.is_closing():
            writer.close()
            await writer.wait_closed()
        logger.info(f"Closed connection from {client_str}")


async def run(started_event: asyncio.Event | None = None):
    server = await asyncio.start_server(handle_connection, "0.0.0.0", port)
    try:
        # handle_connection is called for each new connection established
        addrs = ", ".join(str(sock.getsockname()) for sock in server.sockets)
        logger.info(f"Serving on {addrs}")

        if started_event is not None:
            started_event.set()

        async with server:
            await server.serve_forever()
    except asyncio.CancelledError:
        raise
    finally:
        # clean up server
        server.close()
        await server.wait_closed()
