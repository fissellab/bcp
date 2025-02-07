# Ophiuchus

## Requirements
- CMake 3.24 or later (3.22.1-1ubuntu1 fails with undescriptive error.
    - [Instructions to install a newer version of cmake](https://apt.kitware.com/)
- vcpkg (and `$VCPKG_ROOT` environment variable defined)

## Steps

### Automatic

```
./start.sh
```

### Manual

Clone submodules
```
git submodule update --init --recursive
```

Install dependencies
```
vcpkg install
```

Make Oph your working directory

Configure (re-run this when you change the configuration):
```
cmake --preset=default
```

Build (re-run this when you edit the source code):
```
cmake --build build
```

Run the executable:
```
./build/main
```

## Notes

### vcpkg

This is used to install `nanopb`, a transitive dependency from the `telemetry-uplink` library.