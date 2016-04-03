CC=gcc
CFLAGS=-I.

byzantine_general_problem: byzantine_general_problem.o
	     $(CC) -o byz_demo_app byzantine_general_problem.o -I. -lpthread

clean:
	rm -rf byz_demo_app byzantine_general_problem.o
