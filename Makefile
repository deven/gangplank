#
# Conferencing system server.
#
# Makefile -- commands for building conf server.
#
# Copyright (c) 1992-1993 Deven T. Corzine
#

CC = gcc -Wall -Werror
CFLAGS = -g
LDFLAGS =

EXEC = conf
HDRS = conf.h
SRCS = conf.c
OBJS = $(SRCS:.c=.o)

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $(EXEC) $(OBJS)

$(OBJS): $(HDRS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(EXEC) $(OBJS) core *~
