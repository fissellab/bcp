from pydantic import BaseModel, BeforeValidator, ValidationInfo
from enum import Enum
from typing import Any, Annotated


class Subscribe(BaseModel):
    metric_id: str


class GetMetricIds(BaseModel):
    pass


# special command to stop currently running command
# and all commands recieved before the cancel command
# is recieved
class Cancel(BaseModel):
    pass


# special command to end the current connection,
# stopping the currently running command and forgetting
# all recieved commands over the connection
class End(BaseModel):
    pass


type CommandTypes = Subscribe | GetMetricIds | Cancel | End


class WhichCommandType(str, Enum):
    SUBSCRIBE = "subscribe"
    GET_METRIC_IDS = "get_metric_ids"
    CANCEL = "cancel"
    END = "end"


def is_data_valid(data: Any, info: ValidationInfo) -> CommandTypes:
    # If data is already a command object, return it directly
    if isinstance(data, (Subscribe, GetMetricIds)):
        return data

    if not isinstance(data, dict):
        raise ValueError("Data must be a dictionary or a command object")

    if "which_command" not in info.data:
        raise ValueError("Missing 'which_command' field")

    which_command = info.data["which_command"]
    try:
        if which_command == WhichCommandType.SUBSCRIBE:
            return Subscribe.model_validate(data)
        elif which_command == WhichCommandType.GET_METRIC_IDS:
            return GetMetricIds.model_validate(data)
        elif which_command == WhichCommandType.CANCEL:
            return Cancel.model_validate(data)
        elif which_command == WhichCommandType.END:
            return End.model_validate(data)
        else:
            raise ValueError(f"Invalid command: {which_command}")
    except Exception as e:
        raise ValueError(f"Failed to validate command data: {str(e)}")


class Telecommand(BaseModel):
    which_command: WhichCommandType
    data: Annotated[CommandTypes, BeforeValidator(is_data_valid)]

    @staticmethod
    def from_command(command: CommandTypes) -> "Telecommand":
        which_command = None
        if isinstance(command, Subscribe):
            which_command = WhichCommandType.SUBSCRIBE
        elif isinstance(command, GetMetricIds):
            which_command = WhichCommandType.GET_METRIC_IDS
        elif isinstance(command, Cancel):
            which_command = WhichCommandType.CANCEL
        elif isinstance(command, End):
            which_command = WhichCommandType.END
        else:
            raise ValueError(f"Invalid command: {command}")
        return Telecommand(
            which_command=which_command,
            data=command,
        )
