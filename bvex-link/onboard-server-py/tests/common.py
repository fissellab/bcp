import asyncio
from onboard_server import handle_connection

def dummy_handle_connection(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    pass

class TestServer:
    def __init__(self):
        self.server = None

    async def __aenter__(self):
        self.server = await asyncio.start_server(handle_connection, "127.0.0.1", 8888)
        async with self.server:
            return self

    async def __aexit__(self, exc_type, exc_value, traceback):
        if self.server:
            self.server.close()
            try:
                await self.server.wait_closed()
            except asyncio.CancelledError:
                pass
