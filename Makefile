CC=vc
CFLAGS=-I$(NDK_INC)
all:
	$(CC) +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/openwin.c -o /data/openwin
	$(CC) +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/tiny.c -o /data/tiny
	$(CC) -c +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/serialread.c -o /data/serialread.o
	$(CC) -c +kick13 -c99  -o /data/bde64.o /data/bde64.c

	$(CC) +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/serialread.o /data/bde64.o /data/volumes6.c -o /data/volumes6
	
	#curl --insecure -u 'ozzy:nzv!88^UWAy9YsqWmrI5N$$' -T /data/volumes6 "https://cloud.ozzyboshi.com/nextcloud/remote.php/dav/files/ozzy/Amiga/whdload/volumes6"
	# scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null /data/volumes6 pi@10.0.0.10: 
	# curl  -H "Content-Type: application/json" -X POST -d '{"amigafilename":"games1:volumes6","pcfilename":"/home/pi/volumes6"}' 10.0.0.10:8081/store
	#scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null /data/openwin pi@10.0.0.10: 
	#curl  -H "Content-Type: application/json" -X POST -d '{"amigafilename":"ram:openwin","pcfilename":"/home/pi/openwin"}' 10.0.0.10:8081/store
	#curl 10.0.0.10:8081/exit

	#$(CC) +kick13 -c99 $(CFLAGS) -lamiga -lauto /data/navigate.c -o /data/navigate
