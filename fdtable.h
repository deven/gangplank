// -*- C++ -*-
//
// Conferencing system server.
//
// fdtable.h -- FDTable class interface.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _FDTABLE_H
#define _FDTABLE_H 1

// Include files.
#include "conf.h"

// File descriptor table.
class FDTable {
protected:
   FD **array;				// dynamic array of file descriptors
   int size;				// size of file descriptor table
   int used;				// number of file descriptors used
public:
   FDTable();				// constructor
   ~FDTable();				// destructor
   void OpenListen(int port);		// Open a listening port.
   void OpenTelnet(int lfd);		// Open a telnet connection.
   void Close(int fd);			// Close fd.
   void Select();			// Select across all ready connections.
   void InputReady(int fd);		// Input ready on file descriptor fd.
   void OutputReady(int fd);		// Output ready on file descriptor fd.

   // Send announcement to everyone.  (Formatted write to all connections.)
   void announce(const char *format, ...);

   // Nuke a user (force close connection).
   void nuke(Telnet *telnet, int fd, int drain);

   // Send private message by fd #.
   void SendByFD(Telnet *telnet, int fd, const char *sendlist, int is_explicit,
                 const char *msg);

   // Send public message to everyone.
   void SendEveryone(Telnet *telnet, const char *msg);

   // Send private message by partial name match.
   void SendPrivate(Telnet *telnet, const char *sendlist, int is_explicit,
                    const char *msg);
};

#endif // fdtable.h
