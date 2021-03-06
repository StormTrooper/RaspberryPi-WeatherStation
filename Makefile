CC = gcc
SRC = weather-station.c 
CFLAGS = -Wall
EXE = weather-station
LDFLAGS = -o $(EXE) 
CFDEBUG = -Wall -DDEBUG 
LIBS = -lwiringPi -lm -lpaho-mqtt3c
all:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) $(LIBS)
debug:
	$(CC) $(CFDEBUG) $(LDFLAGS) $(SRC) $(LIBS)
