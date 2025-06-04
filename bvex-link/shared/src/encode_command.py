import json
from typing import TypedDict, Literal


class RetransmitCommand(TypedDict):
    command: Literal["retransmit_segment"]
    sample_id: str
    seq_nums: list[int]


class AckSampleCommand(TypedDict):
    command: Literal["ack_sample"]
    sample_id: str


type Command = RetransmitCommand | AckSampleCommand


def retransmit_segment_command(sample_id: str, seq_nums: list[int]) -> RetransmitCommand:
    return {
        "command": "retransmit_segment",
        "sample_id": sample_id,
        "seq_nums": seq_nums
    }


def ack_sample_command(sample_id: str) -> AckSampleCommand:
    return {
        "command": "ack_sample",
        "sample_id": sample_id
    }


def encode_commands(commands: list[Command]) -> bytes:
    return json.dumps(commands).encode()
