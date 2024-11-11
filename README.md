# bcp
This is bcp the flight control software for the Balloon-borne-VLBI Experiment it is still under development. BCP has two programms for the two flight computers Saggitarius (located in the Sag folder) and Ophiuchus (located in the Oph folder).  The star camera software is a simplified version of the blastcam software found here: https://github.com/BlastTNG/blastcam . The motor control code is in part based on MCP the BLAST flight code: https://github.com/BlastTNG/flight/tree/master/mcp. To run this right now you will need pthread,libconfig, all the star camera dependencies mentioned in the blastcam repo as well as a modified version of SOEM which can be found in the MCP repo 

To compile the Saggitarius version run:
```
cd <flight_code_dir>
./compile_code.sh
```
Then to run do 
```
./bcp_Sag <path_to_bcp_Sag.config>
```
Similalry on Ophiuchus:
```
cd <flight_code_dir>
./compile_code.sh
```
To run do
```
./bcp_Oph <path_to_bcp_Oph.config>
```
