// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// fd.h -- FD class interface.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _FD_H
#define _FD_H 1

// Include files.
#include "fdtable.h"
#include "phoenix.h"

// Types of FD subclasses.
enum FDType {UnknownFD, ListenFD, TelnetFD};

// Data about a particular file descriptor.
class FD {
protected:
   static FDTable fdtable;		// File descriptor table.
public:
   FDType type;				// type of file descriptor
   int fd;				// file descriptor

   static void Select() {		// Select across all ready connections.
      fdtable.Select();
   }
   virtual void InputReady(int fd) = 0;	// Input ready on file descriptor fd.
   virtual void OutputReady(int fd) = 0; // Output ready on file descriptor fd.
   virtual void output(const char *buf) {} // queue output data
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
      fdtable.ReadSelect(fd);
   }
   void NoReadSelect() {		// Do not select fd for reading.
      fdtable.NoReadSelect(fd);
   }
   void WriteSelect() {			// Select fd for writing.
      fdtable.WriteSelect(fd);
   }
   void NoWriteSelect() {		// Do not select fd for writing.
      fdtable.NoWriteSelect(fd);
   }
};

#endif // fd.h
