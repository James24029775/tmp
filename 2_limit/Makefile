.PHONY: clean all

CC = g++
CXXFLAGS = --std=c++14 -Wall -pedantic -pthread -lboost_system
all: hw4.cgi socks_server

hw4.cgi: console.o console_util.o project3_util.o
	$(CC) $(CXXFLAGS) $^ -pthread -o $@

socks_server: socks_server.o util.o
	$(CC) $(CXXFLAGS) $^ -pthread -o $@

clean:
	rm -f *.o hw4.cgi socks_server
