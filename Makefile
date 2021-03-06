#Name: Chenyu Xu & Shiqi Wang
#EMAIL: kenxcy96@gmail.com & sqwang97@ucla.edu
#ID: 204584747 & 304582601

#!/bin/bash

all: server_timer client_timer

server_timer: server_timer.cpp packet.h packet.cpp
	g++ -std=c++11 -Wall -Wextra server_timer.cpp packet.cpp -o server_timer -lrt
	
client_timer: client_timer.cpp packet.h packet.cpp buffer.cpp
	g++ -std=c++11 -Wall -Wextra client_timer.cpp packet.cpp buffer.cpp -o client_timer -lrt

test: packet.h packet.cpp buffer.h buffer.cpp test1.cpp
	g++ -std=c++11 -Wall -Wextra test1.cpp packet.cpp buffer.cpp -o test

clean:
	rm -f server_timer client_timer 204584747.tar.gz received.data *.txt

dist:
	tar -cvzf 204584747.tar.gz server_timer.cpp client_timer.cpp packet.cpp packet.h buffer.cpp buffer.h Makefile report.pdf README
