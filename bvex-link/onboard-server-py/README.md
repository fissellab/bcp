## Setup

[Install Python version 3.13](../python-instructions.md)

Create virtual env, install requirements
`python3.13 -m venv venv
source venv/bin/activate
python -m pip install -r requirements.txt
`

# tests
first start the server
```
python run.py
```
and [start redis](../bcp-redis-client/python/README.md)

then run the tests
```
pytest
```