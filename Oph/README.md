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
cmake --preset=vcpkg
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

## cli

- `print <string>`: Prints the provided string.
- `exit`: Exits the command prompt and shuts down various components if enabled.
- `bvexcam_status`: Displays the status of the bvexcam if it is enabled.
- `focus_bvexcam`: Starts the autofocus mode for bvexcam if it is enabled.
- `accl_status`: Displays the status of the accelerometer if it is enabled.
- `accl_start`: Starts the accelerometer data collection if it is enabled.
- `accl_stop`: Stops the accelerometer data collection if it is enabled.
- `motor_start`: Starts the motor if it is enabled.
- `motor_stop`: Stops the motor if it is enabled.
- `motor_control`: Enters a motor control mode with additional sub-commands:
  - `exit`: Exits the motor control mode.
  - `gotoenc <angle (floating point val)>`: Moves the motor to the specified encoder position.
  - `encdither <start_el,stop_el,vel,nscans>`: Starts an encoder dither scan with the specified parameters.
  - `stop`: Stops the current scan and sets the motor velocity to 0.