BASE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
echo $BASE_DIR
source "$BASE_DIR/Scripts/activate"
pip install -r "$BASE_DIR/requirements.txt"
export PYTHONPATH=$(dirname "$BASE_DIR")  # should be vbex directory
echo "PYTHONPATH=$PYTHONPATH"
