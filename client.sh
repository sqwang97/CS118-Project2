#!/bin/bash

counter=1
until [ $counter -gt 20 ]
do
	echo $counter
	echo $counter >> result_c.txt
	./client_timer localhost 3000 test.txt >> result_c.txt

	echo $counter >> diff_file.txt
	diff received.data test.txt >> diff_file.txt
	sleep 1
	((counter++))
done