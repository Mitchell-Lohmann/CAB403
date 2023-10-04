CFLAGS=-pthread -Wall

overseer:
	gcc -o overseer Overseer.c -Wall 

simulator:
	gcc -o simulator Simulator.c -Wall 

firealarm:
	gcc -o firealarm Firealarm.c -Wall 

cardreader:
	gcc -o cardreader Cardreader.c -Wall 

door:
	gcc -o door Door.c -Wall 

callpoint:
	gcc -o callpoint Callpoint.c -Wall 

tempsensor:
	gcc -o tempsensor Tempsensor.c -Wall 

elevator:
	gcc -o elevator Elevator.c -Wall 

destselect:
	gcc -o destselect Destselect.c -Wall 

camera:
	gcc -o camera Camera.c -Wall 
