#
# Conferencing system server.
#
# Makefile -- commands for building conf server.
#
# Copyright (c) 1992-1993 Deven T. Corzine
#

CC = gcc -Wall -Werror
CXX = g++ -Wall -Werror
CFLAGS = -g
LDFLAGS =

# ESIX:
#CFLAGS = -DUSE_SIGIGNORE
#LDFLAGS = -bsd
#
# Sun:
#CFLAGS = -g -DNEED_STRERROR
#LDFLAGS =

EXEC = conf
HDRS = conf.h
SRCS = conf.cc
OBJS = $(SRCS:.cc=.o)

EXEC2 = restart
HDRS2 =
SRCS2 = restart.c
OBJS2 = $(SRCS2:.c=.o)

all: $(EXEC) $(EXEC2)

$(EXEC): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(EXEC) $(OBJS)

$(EXEC2): $(OBJS2)
	$(CC) $(LDFLAGS) -o $(EXEC2) $(OBJS2)

$(OBJS): $(HDRS)

$(OBJS2): $(HDRS2)

.c.o:
	$(CC) $(CFLAGS) -c $<

.cc.o:
	$(CXX) $(CFLAGS) -c $<

clean:
	rm -f $(EXEC) $(OBJS) $(EXEC2) $(OBJS2) core *~
