# Gebze Technical University
# Computer Engineering
# CSE344 System Programming
# Spring 2021

CSE344 System Programming Coursework SPRING 2021
	
	Tested Environment
        Ubuntu 20.04.2.0 LTS

Homework 1 => advanced file search program for POSIX compatible operating systems.

	How
	The search criteria can be any combination of the following (at least one of them must be
	employed):
	• -f : filename (case insensitive), supporting the following regular expression: +
	• -b : file size (in bytes)
	• -t : file type (d: directory, s: socket, b: block device, c: character device f: regular file, p:
	pipe, l: symbolic link)
	• -p : permissions, as 9 characters (e.g. ‘rwxr-xr--’)
	• -l: number of links

	Usage: 	make
		./myFind -w targetDirectoryPath -f 'lost+file' -b 100 -t b

Homework 2 => Estimate polynomial interpolation using Lagrange form with Signals and processes

	Usage: 	make
		./processM pathToFile

Homework 3 => IPC with fifos and shared memory

	Usage: 	make
		./player –b haspotatoornot –s nameofsharedmemory –f filewithfifonames –m namedsemaphore

Midterm => Simulate COVID-19 vaccination flow with semaphores and processes

	Usage: 	make
		./program –n 3 –v 2 –c 3 –b 11 –t 3 –i inputfilepath 

Homework 4 => Initiation to POSIX threads 

	Usage: 	make
		./program homeworkFilePath studentsFilePath 10000 

Final => Implemantation of a Client-Server database management systems like Oracle, IBM, SQL etc. with sockets and threads
	
	Implemented SQLs:
		SELECT
		SELECT DISTINCT
		UPDATE
		(WHERE condition)

	Server is a daemon process
	Usage: 	make
		./server -p PORT -o pathToLogFile –l poolSize –d datasetPath
		./client –i id -a 127.0.0.1 -p PORT -o pathToQueryFile