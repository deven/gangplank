// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// listen.h -- Listen class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _LISTEN_H
#define _LISTEN_H 1

// Include files.
#include "fd.h"
#include "fdtable.h"
#include "phoenix.h"

// Listening socket (subclass of FD).
class Listen: public FD {
protected:
   void RequestShutdown(int port);	// Connect to port, request shutdown.
public:
   static void Open(int port);		// Open a listening port.
   Listen(int port);			// constructor
   ~Listen();				// destructor
   void InputReady(int fd) {		// Input ready on file descriptor fd.
      fdtable.OpenTelnet(fd);		// Accept pending telnet connection.
   }
   void OutputReady(int fd) {		// Output ready on file descriptor fd.
      error("Listen::OutputReady(fd = %d): invalid operation!", fd);
   }
   void Closed();			// Connection is closed.
};

#endif // listen.h
