CC=gcc
CFLAGS=-ggdb -Wall

all: wc main map

wc: wc.c
	$(CC) $(CFLAGS) -o wc wc.c
main: main.c
	$(CC) $(CFLAGS) -o main main.c

map: map.c
	$(CC) $(CFLAGS) -o map map.c

clean:
	rm -f wc.o map.o main.o wc main map
