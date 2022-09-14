# cfile generation tool
#

CC=gcc
CFLAGS=-Wall
LFLAGS=
OBJ=main.c
BIN=fsobs

all:
	$(CC) $(OBJ) -o $(BIN)

install:
	@cp $(BIN) /usr/sbin/
	@cp cfile.lic /etc/


