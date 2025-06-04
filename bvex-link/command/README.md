## Ubuntu installation

Install Python version 3.13
```
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt update
sudo apt install python3.13
sudo apt install python3.13-venv
```

Create virtual env, install requirements
`python3.13 -m venv venv
source venv/bin/activate
python -m pip install -r requirements.txt
`

Running
```bash
python src/server.py
```

src/generated/nano_pb2 can be built by running make in https://github.com/nanopb/nanopb/tree/master/generator/proto
OR downloading prebuilt nanopb binaries

Build nanopb headers:
```bash
protoc -I/home/oliver/devtools/nanopb/generator -I/home/oliver/devtools/nanopb/generator/proto -I/home/oliver/devtools/nanopb/generator/proto/google/protobuf -I../shared --python_out=./src/generated --pyi_out=./src/generated sample.proto nanopb.proto
```