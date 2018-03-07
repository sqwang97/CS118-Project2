#!/bin/bash

counter=1
until [ $counter -gt 50 ]
do
	echo $counter
	./server 3000 >> result_s.txt
	((counter++))
done