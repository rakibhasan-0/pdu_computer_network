CC=gcc
CFLAGS = -std=gnu11 -Wall -Wextra


table_mwe: hashtable.o table_mwe.c
	$(CC) $(CFLAGS) hashtable.o table_mwe.c -o table_mwe

hashtable.o: hashtable.c hashtable.h
	$(CC) $(CFLAGS) -c hashtable.c -o hashtable.o
