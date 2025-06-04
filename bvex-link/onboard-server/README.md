You must determine MTU manually, `ping [command-ip] -f -l 1492` and keep lowering num bytes until no fragmentation needed.

TODO: add this https://github.com/marketplace/actions/doxygen-github-pages-deploy-action


Required packages:
vcpkg
cmake
ninja-build

To use this, you must have vcpkg and have $VCPKG_ROOT defined

`cmake --preset=debug`
`cmake --build build`
`./build/main`

TODO: build headers with cmake instead of manual

Build onboard telemetry nanopb headers:
```bash
~/devtools/nanopb/generator/nanopb_generator.py --output-dir=./src/codec/onboard-tm/pb_generated --proto-path=../onboard-telemetry-client primitive.proto sample.proto
```

Build requests nanopb headers:
```bash
~/devtools/nanopb/generator/nanopb_generator.py --output-dir=./src/codec/requests/pb_generated --proto-path=../onboard-telemetry-client primitive.proto request.proto response.proto
```
