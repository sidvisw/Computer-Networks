libmsocket.a: mysocket.o
	ar -rcs libmsocket.a mysocket.o

mysocket.o: mysocket.c mysocket.h
	gcc -Wall -c mysocket.c

tests: libmsocket.a test_server.c test_client.c
	gcc -Wall test_server.c -L. -lmsocket -lpthread -o test_server
	gcc -Wall test_client.c -L. -lmsocket -lpthread -o test_client

clean:
	rm mysocket.o

distclean:
	rm libmsocket.a mysocket.o test_server test_client client.log server.log

