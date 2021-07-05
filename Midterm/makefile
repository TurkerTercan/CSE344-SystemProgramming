all: program

program: clinic.c nurse vaccinator citizen data_types.h
	gcc clinic.c -o program -Wall -ggdb3 -lrt -lpthread -pipe

citizen: citizen.c
	gcc citizen.c -o citizen -Wall -ggdb3 -lrt -lpthread -pipe

nurse: nurse.c
	gcc nurse.c -o nurse -Wall -ggdb3 -lrt -lpthread -pipe

vaccinator: vaccinator.c data_types.h
	gcc vaccinator.c -o vaccinator -Wall -ggdb3 -lrt -lpthread -pipe

clean:
	rm program
	rm nurse
	rm vaccinator
	rm citizen