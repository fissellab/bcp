from src import server
from dotenv import load_dotenv, dotenv_values
import asyncio
import os

load_dotenv()
config = dotenv_values(".env")
print(config)
asyncio.run(server.run_server())