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
#include "phoenix.h"

// Types of FD subclasses.
enum FDType {UnknownFD, ListenFD, TelnetFD};

// Data about a particular file descriptor.
class FD {
public:
   FDType type;				// type of file descriptor
   int fd;				// file descriptor

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
      FD_SET(fd, &readfds);
   }
   void NoReadSelect() {		// Do not select fd for reading.
      FD_CLR(fd, &readfds);
   }
   void WriteSelect() {			// Select fd for writing.
      FD_SET(fd, &writefds);
   }
   void NoWriteSelect() {		// Do not select fd for writing.
      FD_CLR(fd, &writefds);
   }
};

#endif // fd.h
