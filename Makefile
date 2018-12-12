##########################
# Makefile
# Author: Kevin Littell
# Date: Dec 12, 2018
# COSC 3750, homework 11
# makes a program using the listed dependencies
##########################

CC=gcc
CFLAGS=-ggdb -Wall -std=gnu99
RM=/bin/rm -f

.PHONEY: clean

mmult:
	${CC} ${CFLAGS} mmult.c -o mmult -lpthread

clean:
	${RM} mmult
