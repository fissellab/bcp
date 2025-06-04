import pytest
from bvex_codec.telecommand import (
    Telecommand,
    WhichCommandType,
    Subscribe,
    GetMetricIds,
)


def test_valid_subscribe_command():
    """Test creating a valid Subscribe command"""
    cmd = Telecommand.from_command(Subscribe(metric_id="test_metric"))
    assert isinstance(cmd.data, Subscribe)
    assert cmd.data.metric_id == "test_metric"
    assert cmd.which_command == WhichCommandType.SUBSCRIBE


def test_valid_request_metric_ids_command():
    """Test creating a valid RequestMetricIds command"""
    cmd = Telecommand.from_command(GetMetricIds())
    assert isinstance(cmd.data, GetMetricIds)
    assert cmd.which_command == WhichCommandType.GET_METRIC_IDS


def test_invalid_command_type():
    """Test that invalid command types are rejected"""
    with pytest.raises(ValueError):
        Telecommand.model_validate({"which_command": "invalid_command", "data": {}})


def test_missing_which_command():
    """Test that missing which_command field is caught"""
    with pytest.raises(ValueError):
        Telecommand.model_validate(
            {
                "data": {"invalid": "data"},
            }
        )


def test_invalid_subscribe_data():
    """Test that invalid Subscribe command data is rejected"""
    with pytest.raises(ValueError):
        Telecommand.model_validate(
            {
                "which_command": WhichCommandType.SUBSCRIBE,
                "data": {},  # Missing required metric_id
            }
        )
