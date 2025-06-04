# Redis server

Start redis server on port 6379.
- [Install docker](https://docs.docker.com/engine/install/ubuntu/#install-using-the-repository)
- Create and start: `sudo docker run -d --name redis -p 6379:6379 redis:7.4`
- Start: `sudo docker start redis`

# Redis devtools
Redis insight:
```bash
sudo docker run -d --name redisinsight -p 5540:5540 redis/redisinsight:latest
xdg-open http://localhost:5540
```
Redis CLI:
```
sudo docker exec -it redis redis-cli
```
Command FLUSHALL resets all databases.
