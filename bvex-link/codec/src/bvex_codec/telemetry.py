from pydantic import BaseModel, BeforeValidator, ValidationInfo
from enum import Enum
from typing import Any, Annotated, List
from bvex_codec.sample import Sample


class MetricIds(BaseModel):
    metric_ids: List[str]


type TMMessage = Sample | MetricIds


class WhichTMMessageType(str, Enum):
    METRIC_IDS = "metric_ids"
    SAMPLE = "sample"


def is_data_valid(data: Any, info: ValidationInfo) -> TMMessage:
    # If data is already a command object, return it directly
    if isinstance(data, (Sample, MetricIds)):
        return data

    if not isinstance(data, dict):
        raise ValueError("Data must be a dictionary or a command object")

    if "which_type" not in info.data:
        raise ValueError("Missing 'which_type' field")

    which_type = info.data["which_type"]
    try:
        if which_type == WhichTMMessageType.METRIC_IDS:
            return MetricIds.model_validate(data)
        elif which_type == WhichTMMessageType.SAMPLE:
            return Sample.model_validate(data)
        else:
            raise ValueError(f"Invalid telemetry: {which_type}")
    except Exception as e:
        raise ValueError(f"Failed to validate telemetry data: {str(e)}")


class Telemetry(BaseModel):
    which_type: WhichTMMessageType
    data: Annotated[TMMessage, BeforeValidator(is_data_valid)]

    @staticmethod
    def from_telemetry(telemetry: TMMessage) -> "Telemetry":
        which_type = None
        if isinstance(telemetry, MetricIds):
            which_type = WhichTMMessageType.METRIC_IDS
        elif isinstance(telemetry, Sample):
            which_type = WhichTMMessageType.SAMPLE
        else:
            raise ValueError(f"Invalid telemetry: {telemetry}")
        return Telemetry(
            which_type=which_type,
            data=telemetry,
        )
