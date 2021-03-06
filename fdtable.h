// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// fdtable.h -- FDTable class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _FDTABLE_H
#define _FDTABLE_H 1

// Include files.
#include "object.h"
#include "phoenix.h"

// File descriptor table.
class FDTable {
protected:
   static fd_set readfds;		// read fdset for select()
   static fd_set writefds;		// write fdset for select()
   Pointer<FD> *array;			// dynamic array of file descriptors
   int size;				// size of file descriptor table
   int used;				// number of file descriptors used
public:
   FDTable();				// constructor
   ~FDTable();				// destructor
   void OpenListen(int port);		// Open a listening port.
   void OpenTelnet(int lfd);		// Open a telnet connection.
   Pointer<FD> Closed(int fd);		// Close fd, return FD object pointer.
   void Close(int fd);			// Close fd, deleting FD object.
   void CloseAll();			// Close all fds.
   void Select();			// Select across all ready connections.
   void InputReady(int fd);		// Input ready on file descriptor fd.
   void OutputReady(int fd);		// Output ready on file descriptor fd.

   // Select fd for reading.
   void ReadSelect(int fd) {
      FD_SET(fd, &readfds);
   }

   // Do not select fd for reading.
   void NoReadSelect(int fd) {
      FD_CLR(fd, &readfds);
   }

   // Select fd for writing.
   void WriteSelect(int fd) {
      FD_SET(fd, &writefds);
   }

   // Do not select fd for writing.
   void NoWriteSelect(int fd) {
      FD_CLR(fd, &writefds);
   }
};

#endif // fdtable.h
