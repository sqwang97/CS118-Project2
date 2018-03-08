#!/bin/bash

counter=1
until [ $counter -gt 50 ]
do
	echo $counter
	./client localhost 3000 server.cpp 3000 >> result_c.txt
	sleep 1
	((counter++))
done