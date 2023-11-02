all: client 

client: client.o
	gcc -o client client.o library.o -pthread

client.o: client.c library.c
	gcc -c -g client.c library.c

clean: 
	rm *.o

