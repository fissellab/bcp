#!/bin/bash

# requires python3
# call with -h for help

CALL_DIR=$(pwd)

PROTO_PATH=""
OUTPUT_DIR=""
FILENAMES=()

USAGE="Usage: $(basename $0) [-p proto_path] [-o output_dir] <filename1> <filename2> ..."

# Parse command-line arguments
while getopts 'p:o:h' opt; do
  case "$opt" in
    p)
      PROTO_PATH="$OPTARG"
      ;;
    o)
      OUTPUT_DIR="$OPTARG"
      ;;
    h|?)
      echo $USAGE
      exit 1
      ;;
  esac
done
shift "$(($OPTIND -1))"

FILENAMES=("$@")

Check if at least one filename is provided
if [ ${#FILENAMES[@]} -eq 0 ]; then
    echo $USAGE
    exit 1
fi

if [ -z "${PROTO_PATH}" ]; then
    echo "PROTO_PATH is not set. Using current directory."
    PROTO_PATH="."
fi

if [ -z "${OUTPUT_DIR}" ]; then
    echo "OUTPUT_DIR is not set. Using current directory."
    OUTPUT_DIR="."
fi

cd /tmp

# Can be changed to diff. version
DL_NAME="nanopb-0.4.9.tar.gz"

TMP_NANOPB_DIR="nanopb-tmp"

# Download the nanopb tarball
curl -O https://jpa.kapsi.fi/nanopb/download/${DL_NAME}

# Create a temporary directory for extraction
mkdir -p ${TMP_NANOPB_DIR}

# Extract the tarball to the temporary directory
tar -xzf ${DL_NAME} -C ${TMP_NANOPB_DIR} --strip-components=1

# Delete the tarball
rm ${DL_NAME}

TMP_VENV_NAME="temp_nanopb_generation_venv"

python3 -m venv ${TMP_VENV_NAME}

source ${TMP_VENV_NAME}/bin/activate

pip install protobuf grpcio-tools  # nanopb_generator.py dependencies

cd ${CALL_DIR}

"/tmp/${TMP_NANOPB_DIR}/generator/nanopb_generator.py" \
    --proto-path=${PROTO_PATH} --output-dir=${OUTPUT_DIR} \
    ${FILENAMES[@]}

deactivate