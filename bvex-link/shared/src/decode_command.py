from shared.src.encode_command import Command
import json

# assumes all commands are retransmit data
# command datagram
# 
def decode_commands(data: bytes) -> list[Command]:
    print(data)
    return json.loads(data.decode())