// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// fd.h -- FD class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _FD_H
#define _FD_H 1

// Include files.
#include "fdtable.h"
#include "object.h"
#include "phoenix.h"

// Types of FD subclasses.
enum FDType {UnknownFD, ListenFD, TelnetFD};

// Data about a particular file descriptor.
class FD: public Object {
protected:
   static FDTable fdtable;		// File descriptor table.
public:
   FDType type;				// type of file descriptor
   int fd;				// file descriptor

   static void Select() {		// Select across all ready connections.
      fdtable.Select();
   }
   virtual void InputReady() = 0;	// Input ready on file descriptor fd.
   virtual void OutputReady() = 0;	// Output ready on file descriptor fd.
   virtual void Closed() = 0;		// Connection is closed.
   virtual ~FD() {}			// destructor
   void NonBlocking() {			// Place fd in non-blocking mode.
      int flags;

      if ((flags = fcntl(fd, F_GETFL)) < 0) {
         error("FD::NonBlocking(): fcntl(F_GETFL)");
      }
      flags |= O_NONBLOCK;
      if (fcntl(fd, F_SETFL, flags) == -1) {
         error("FD::NonBlocking(): fcntl(F_SETFL)");
      }
   }
   void ReadSelect() {			// Select fd for reading.
      if (fd != -1) fdtable.ReadSelect(fd);
   }
   void NoReadSelect() {		// Do not select fd for reading.
      if (fd != -1) fdtable.NoReadSelect(fd);
   }
   void WriteSelect() {			// Select fd for writing.
      if (fd != -1) fdtable.WriteSelect(fd);
   }
   void NoWriteSelect() {		// Do not select fd for writing.
      if (fd != -1) fdtable.NoWriteSelect(fd);
   }
};

#endif // fd.h
