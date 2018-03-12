#!/bin/bash

counter=1
until [ $counter -gt 20 ]
do
	echo $counter
	echo $counter >> result_s.txt
	./server_timer 3000 >> result_s.txt
	((counter++))
done