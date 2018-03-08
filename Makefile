#Name: Chenyu Xu & Shiqi Wang
#EMAIL: kenxcy96@gmail.com & sqwang97@ucla.edu
#ID: 204584747 & 304582601

#!/bin/bash

all: server client

server: server.cpp packet.h packet.cpp buffer.h buffer.cpp
	g++ -std=c++11 -Wall -Wextra server.cpp packet.cpp -o server

client: client.cpp packet.h packet.cpp buffer.h buffer.cpp
	g++ -std=c++11 -Wall -Wextra client.cpp packet.cpp buffer.cpp -o client

test: packet.h packet.cpp buffer.h buffer.cpp test1.cpp
	g++ -std=c++11 -Wall -Wextra test1.cpp packet.cpp buffer.cpp -o test

clean:
	rm -f server client 204584747.tar.gz received.data *.txt

dist:
	tar -cvzf 204584747.tar.gz server.cpp client.cpp packet.cpp packet.h buffer.cpp buffer.h Makefile report.pdf README
