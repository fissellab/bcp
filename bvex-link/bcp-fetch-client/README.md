# BCP Fetch

Provides a C library for sending and requesting data. Intended to be consumed by [BVEX's onboard computer code](https://github.com/fissellab/bcp).

## Building

### Requirements

- `vcpkg`, `$VCPKG_ROOT` defined

### Steps

- `vcpkg install`
- `cmake --preset=vcpkg`
- `cmake --build build`

## Consuming bcp-fetch

- Requires transitive dependency `nanopb`
- Easiest way to do this is with `vcpkg` and `cmake`
    - [Integrate consumer project with vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-bash) and add `nanopb` to your manifest dependencies
    - Add this project's directory with `add_subdirectory`
    - You can now link `bcp-fetch` to your targets

## Generating new nanopb headers
```bash
pip install protobuf grpcio-tools
```
```bash
~/devtools/nanopb/generator/nanopb_generator.py --output-dir=./src/generated/nanopb primitive.proto sample.proto request.proto response.proto
```

## Run unit tests

```bash
cmake --preset=debug
cmake --build build
./build/gtest
```

## Manual testing
First, run the onboard server, then build the library with tests:
```bash
cmake --preset=debug
cmake --build build
```

Test sample sending:
```bash
./build/test sample testfloat float 12.0
./build/test sample teststring string hello
./build/test sample teststring file /home/oliver/dev/bvex-link/test-samples/boat.png png
```

Test requests sending:
```bash
./build/test sample testfloat float 12.0
./build/test request testfloat float

./build/test sample teststring string hello
./build/test request teststring string
```