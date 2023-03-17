libmsocket.a: mysocket.o
	ar -rcs libmsocket.a mysocket.o

mysocket.o: mysocket.c mysocket.h
	gcc -Wall -c mysocket.c

clean:
	rm *.o test_server test_client

distclean:
	rm  *.o libmsocket.a test_server test_client