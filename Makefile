CC=gcc

P=web-manager

all: $(P) http.o http_post.o

$(P): $(P).c http.o http_post.o $(P).h
	$(CC) -Wall $(P).c http.o http_post.o -o $(P)

http.o: http.c http.h
	$(CC) -Wall -c http.c -o http.o

http_post.o: http_post.c http.o http_post.h
	$(CC) -Wall -c http_post.c -o http_post.o

clean:
	rm -f $(P) *.o

