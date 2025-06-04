from pydantic import BaseModel, field_validator, ValidationInfo, field_serializer
from typing import Union, Any
from enum import Enum
import os
import base64

PrimitiveType = Union[int, float, str, bool]


class WhichPrimitive(str, Enum):
    INT = "int"
    FLOAT = "float"
    STR = "str"
    BOOL = "bool"

    @staticmethod
    def from_value(value: Any) -> "WhichPrimitive":
        if isinstance(value, bool):
            return WhichPrimitive.BOOL
        elif isinstance(value, int):
            return WhichPrimitive.INT
        elif isinstance(value, float):
            return WhichPrimitive.FLOAT
        elif isinstance(value, str):
            return WhichPrimitive.STR
        else:
            raise ValueError(f"Invalid primitive type: {type(value)}")


class PrimitiveData(BaseModel):
    which_primitive: WhichPrimitive
    value: PrimitiveType

    @staticmethod
    def from_value(value: PrimitiveType) -> "PrimitiveData":
        return PrimitiveData(
            which_primitive=WhichPrimitive.from_value(value), value=value
        )

    @field_validator("value")
    @classmethod
    def validate_value_type(cls, v: Any, info: ValidationInfo) -> PrimitiveType:
        if "which_primitive" not in info.data:
            return v

        which_primitive = info.data["which_primitive"]
        try:
            if which_primitive == WhichPrimitive.INT:
                return int(v)
            elif which_primitive == WhichPrimitive.FLOAT:
                return float(v)
            elif which_primitive == WhichPrimitive.STR:
                return str(v)
            elif which_primitive == WhichPrimitive.BOOL:
                return bool(v)
            else:
                assert False, f"Invalid primitive type: {which_primitive}"
        except Exception as e:
            raise ValueError(
                f"Failed to coerce value to {which_primitive}: {str(e)}"
            ) from e
        return v


class FileData(BaseModel):
    filename: str
    data: bytes

    @field_serializer("data")
    def serialize_data(self, data: bytes, _info) -> str:
        return base64.b64encode(data).decode("utf-8")

    @field_validator("data", mode="before")
    @classmethod
    def validate_data(cls, v: Any) -> bytes:
        if isinstance(v, str):
            return base64.b64decode(v.encode("utf-8"))
        elif isinstance(v, bytes):
            return v
        elif isinstance(v, bytearray):
            return bytes(v)
        else:
            raise ValueError(f"Expected str or bytes for data field, got {type(v)}")

    @staticmethod
    def from_file_path(file_path: str) -> "FileData":
        path_parts = os.path.splitext(file_path)
        if len(path_parts) == 1:
            extension = ""
            filename = path_parts[0]
        else:
            extension = path_parts[-1]
            filename = os.path.basename(file_path)
        with open(file_path, "rb") as f:
            data = f.read()
        return FileData(filename=filename, data=data)


class WhichDataType(str, Enum):
    PRIMITIVE = "primitive"
    FILE = "file"


class SampleMetadata(BaseModel):
    metric_id: str
    timestamp: float | int
    which_data_type: WhichDataType


class Sample(BaseModel):
    metadata: SampleMetadata
    data: Union[PrimitiveData, FileData]

    # decode json: Sample.model_validate_json

    # encode json: Sample.model_dump_json

    @field_validator("data")
    @classmethod
    def validate_data_type(
        cls, v: Union[PrimitiveData, FileData], info: ValidationInfo
    ) -> Union[PrimitiveData, FileData]:
        if "metadata" not in info.data:
            return v

        which_data_type = info.data["metadata"].which_data_type
        if which_data_type == WhichDataType.PRIMITIVE and not isinstance(
            v, PrimitiveData
        ):
            raise ValueError(
                f"Expected PrimitiveData for PRIMITIVE data type, got {type(v)}"
            )
        elif which_data_type == WhichDataType.FILE and not isinstance(v, FileData):
            raise ValueError(f"Expected FileData for FILE data type, got {type(v)}")
        return v
