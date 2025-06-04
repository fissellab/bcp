import redis
from bcp_redis_client.sample import set_sample_primitive
import math
import time

r = redis.Redis()
metric_id = "test"
while True:
    set_sample_primitive(r, metric_id, math.sin(time.time()/math.pi))
    time.sleep(1)