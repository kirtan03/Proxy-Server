all: proxy

proxy: proxy.o
	gcc -o proxy proxy.o library.o -pthread

proxy.o: proxy.c library.c
	gcc -c -g proxy.c library.c 

clean: 
	rm *.o
