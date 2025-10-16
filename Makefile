CC=gcc

P=web-manager

all: $(P) http.o

$(P): $(P).c http.o http_filemanager.c killall.c $(P).h
	$(CC) -Wall $(P).c http.o -o $(P)
	strip $(P)

http.o: http.c http.h
	$(CC) -Wall -c http.c -o http.o

clean:
	rm -f $(P) *.o

