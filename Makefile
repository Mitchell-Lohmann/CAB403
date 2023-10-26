CFLAGS=-pthread -Wall

overseer: Overseer.c
	gcc -o overseer Overseer.c common.c -Wall

simulator: Simulator.c
	gcc -o simulator Simulator.c common.c -Wall 

firealarm: Firealarm.c
	gcc -o firealarm Firealarm.c common.c -Wall 

cardreader: Cardreader.c 
	gcc -o cardreader Cardreader.c common.c -Wall 

door: Door.c
	gcc -o door Door.c common.c -Wall 

callpoint: Callpoint.c
	gcc -o callpoint Callpoint.c common.c -Wall 

tempsensor: Tempsensor.c
	gcc -o tempsensor Tempsensor.c common.c -Wall 

elevator: Elevator.c
	gcc -o elevator Elevator.c common.c -Wall 

destselect: Destselect.c
	gcc -o destselect Destselect.c common.c -Wall 

camera: Camera.c
	gcc -o camera Camera.c common.c -Wall 
