import pytest
import redis.asyncio as redis
from bcp_redis_client.asyncio import get_all_metric_ids
import asyncio

@pytest.mark.asyncio
async def test_get_all_metric_ids():
    r = redis.Redis(host="localhost", port=6379, db=0)

    set_metric_ids = ["test_metric_1", "test_metric_2", "test_metric_3"]
    for metric_id in set_metric_ids:
        await r.set(
            f"sample-cache:{metric_id}",
            1,
        )
    
    await asyncio.sleep(1)

    retrieved_metric_ids = await get_all_metric_ids(r)
    assert set(set_metric_ids).issubset(retrieved_metric_ids)
