# Minimal Makefile for Linux (GTK4)
CC=gcc
CFLAGS=`pkg-config --cflags gtk4` -O2 -Wall -Wextra
LDFLAGS=`pkg-config --libs gtk4`

all: client_gui

client_gui: client_gui.o ft_core.o
	$(CC) -o $@ $^ $(LDFLAGS)

client_gui.o: client_gui.c ft_core.h
ft_core.o: ft_core.c ft_core.h

clean:
	rm -f *.o client_gui
