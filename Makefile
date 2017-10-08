CC=vc
CFLAGS=-I$(NDK_INC)
all:
	$(CC) -c +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/amiga_operations.c -o /data/amiga_operations.o
	$(CC) -c +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/serialread.c -o /data/serialread.o
	$(CC) -c +kick13 -c99  -o /data/bde64.o /data/bde64.c

	$(CC) +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/amiga_operations.o /data/serialread.o /data/bde64.o  /data/alsfssrv.c -o /data/alsfssrv
	

