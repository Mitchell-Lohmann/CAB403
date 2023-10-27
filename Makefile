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


kill:
	pkill -f overseer 
	pkill -f firealarm
	pkill -f cardreader
	pkill -f door
	pkill -f callpoint
	pkill -f tempsensor
