#!/bin/bash
./client -i 1 -a 127.0.0.1 -p 6900 -o queries.txt > clientLog1.txt &
./client -i 2 -a 127.0.0.1 -p 6900 -o queries.txt > clientLog2.txt &
./client -i 3 -a 127.0.0.1 -p 6900 -o queries.txt > clientLog3.txt &
./client -i 4 -a 127.0.0.1 -p 6900 -o queries.txt > clientLog4.txt &
for (( c=0; c<=63; c++ ))
do
  ./client -i 1 -a 127.0.0.1 -p 6900 -o queries.txt >> clientLog5.txt &
  ./client -i 2 -a 127.0.0.1 -p 6900 -o queries.txt >> clientLog6.txt &
  ./client -i 3 -a 127.0.0.1 -p 6900 -o queries.txt >> clientLog7.txt &
  ./client -i 4 -a 127.0.0.1 -p 6900 -o queries.txt >> clientLog8.txt &
done
wait
