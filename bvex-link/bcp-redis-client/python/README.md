# Install dependencies

```bash
python3.13 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

# Tests
[Start redis server](../../redis-instructions.md)

```bash
pip install -e .
python -m pytest test/test_sample.py
```



# Production
Redis daemon
