all:
	gcc -Wall -o weather-station weather-station.c lol_dht22/dht22.c lol_dht22/locking.c -lwiringPi -lcurl -lm
