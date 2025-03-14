# bcp
This is bcp the flight control software for the Balloon-borne-VLBI Experiment it is still under development. BCP has two programms for the two flight computers Saggitarius (located in the Sag folder) and Ophiuchus (located in the Oph folder).  The star camera software is a simplified version of the blastcam software found here: https://github.com/BlastTNG/blastcam . The motor control code is in part based on MCP the BLAST flight code: https://github.com/BlastTNG/flight/tree/master/mcp. To run this right now you will need pthread,libconfig, all the star camera dependencies mentioned in the blastcam repo as well as a modified version of SOEM which can be found in the MCP repo 

## Compiling on Sagittarius
```
cd <flight_code_dir>
./compile_code.sh
```
Then to run do 
```
./bcp_Sag <path_to_bcp_Sag.config>
```
## Building on Ophiuchus
- View [./Oph/README.md](./Oph/README.md)

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