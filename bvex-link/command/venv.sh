#! /bin/bash
# cd should be bvex-link/command directory

cd "$(dirname "${BASH_SOURCE[0]}")"
source ./venv/bin/activate
pip install -r "./requirements.txt"
export PYTHONPATH=$(pwd):$(pwd)/generated  
echo "PYTHONPATH=$PYTHONPATH"
