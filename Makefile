# -*- Makefile -*-
#
# Phoenix conferencing system server.
#
# Makefile -- commands for building Phoenix server.
#
# Copyright (c) 1992-1993 Deven T. Corzine
#

CC = gcc -Wall -Werror
CXX = g++ -Wall -Werror
CFLAGS = -g
LDFLAGS =
LIBS = -lcrypt

# ESIX:
#CFLAGS = -DUSE_SIGIGNORE -DNO_BOOLEAN
#LDFLAGS = -bsd
#
# Sun:
#CFLAGS = -g -DNEED_STRERROR
#LDFLAGS =

EXEC = phoenixd
HDRS = block.h fd.h fdtable.h line.h listen.h name.h outbuf.h output.h \
       outstr.h phoenix.h session.h telnet.h user.h
SRCS = fdtable.cc listen.cc output.cc outstr.cc phoenix.cc session.cc \
       telnet.cc user.cc
OBJS = $(SRCS:.cc=.o)

EXEC2 = restart
HDRS2 =
SRCS2 = restart.c
OBJS2 = $(SRCS2:.c=.o)

all: $(EXEC) $(EXEC2)

$(EXEC): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

$(EXEC2): $(OBJS2)
	$(CC) $(LDFLAGS) -o $(EXEC2) $(OBJS2) $(LIBS)

$(OBJS): $(HDRS)

$(OBJS2): $(HDRS2)

.c.o:
	$(CC) $(CFLAGS) -c $<

.cc.o:
	$(CXX) $(CFLAGS) -c $<

clean:
	rm -f $(EXEC) $(OBJS) $(EXEC2) $(OBJS2) core *~
