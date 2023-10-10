CFLAGS=-pthread -Wall

overseer: Overseer.c
	gcc -o overseer Overseer.c  -Wall 

simulator:
	gcc -o simulator Simulator.c common.c -Wall 

firealarm:
	gcc -o firealarm Firealarm.c common.c -Wall 

cardreader:
	gcc -o cardreader Cardreader.c common.c -Wall 

door: Door.c
	gcc -o door Door.c common.c -Wall 

callpoint:
	gcc -o callpoint Callpoint.c common.c -Wall 

tempsensor:
	gcc -o tempsensor Tempsensor.c common.c -Wall 

elevator:
	gcc -o elevator Elevator.c common.c -Wall 

destselect:
	gcc -o destselect Destselect.c common.c -Wall 

camera:
	gcc -o camera Camera.c common.c -Wall 
