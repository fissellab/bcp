# bcp
This is bcp the flight controll software for the Balloon-borne-VLBI Experiment it is still under development. BCP has two programms for the two flight computers Saggitarius (located in the Sag folder) and Ophiuchus (located in the Oph folder). To run this right now you will ned pthread as well as libconfig To compile the Saggitarius version run:
```
cd <flight_code_dir>
gcc -o bcp_Sag main_Sag.c file_io_Sag.c cli_Sag.c -lpthread -lconfig
```
Then to run do 
```
./bcp_Sag <path_to_bcp_Sag.config>
```
Similalry on Ophiuchus:
```
cd <flight_code_dir>
gcc -o bcp_Oph main_Oph.c file_io_Oph.c cli_Oph.c -lpthread -lconfig
```
To run do
```
./bcp_Oph <path_to_bcp_Oph.config>
```
