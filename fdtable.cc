// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// fdtable.cc -- FDTable class implementation.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Include files.
#include "fdtable.h"
#include "listen.h"
#include "name.h"
#include "outbuf.h"
#include "output.h"
#include "outstr.h"
#include "phoenix.h"
#include "session.h"
#include "telnet.h"
#include "user.h"

FDTable FD::fdtable;			// File descriptor table.
fd_set FDTable::readfds;		// read fdset for select()
fd_set FDTable::writefds;		// write fdset for select()

FDTable::FDTable()			// constructor
{
   FD_ZERO(&readfds);
   FD_ZERO(&writefds);
   used = 0;
   size = getdtablesize();
   array = new Pointer<FD>[size];
   for (int i = 0; i < size; i++) array[i] = NULL;
}

FDTable::~FDTable()			// destructor
{
   delete[] array;
}

void FDTable::OpenListen(int port)	// Open a listening port.
{
   Pointer<Listen> l(new Listen(port));
   if (l->fd == -1) return;
   if (l->fd >= used) used = l->fd + 1;
   array[l->fd] = l;
   l->ReadSelect();
}

void FDTable::OpenTelnet(int lfd)	// Open a telnet connection.
{
   Pointer<Telnet> t(new Telnet(lfd));
   if (t->fd == -1) return;
   if (t->fd >= used) used = t->fd + 1;
   array[t->fd] = t;
}

Pointer<FD> FDTable::Closed(int fd)	// Close fd, return FD object pointer.
{
   if (fd < 0 || fd >= used) return NULL;
   Pointer<FD> FD(array[fd]);
   array[fd] = NULL;
   if (fd == used - 1) {		// Fix highest used index if necessary.
      while (used > 0) {
         if (array[--used]) {
            used++;
            break;
         }
      }
   }
   return FD;
}

void FDTable::Close(int fd)		// Close fd, deleting FD object.
{
   Pointer<FD> FD(Closed(fd));
   if (FD) FD->Closed();
}

void FDTable::CloseAll() {		// Close all fds.
   for (int i = 0; i < used; i++) Close(i);
   used = 0;
}

void FDTable::Select()			// Select across all ready connections.
{
   fd_set rfds = readfds;		// copy of readfds to pass to select()
   fd_set wfds = writefds;		// copy of writefds to pass to select()
   int found;				// number of file descriptors found

   found = select(size, &rfds, &wfds, NULL, NULL);

   if (found == -1) {
      if (errno == EINTR) return;
      error("FDTable::Select(): select()");
   }

   // Check for I/O ready on connections.
   for (int fd = 0; found && fd < used; fd++) {
      if (FD_ISSET(fd, &rfds)) {
         InputReady(fd);
         found--;
      }
      if (FD_ISSET(fd, &wfds)) {
         OutputReady(fd);
         found--;
      }
   }
}

void FDTable::InputReady(int fd)	// Input ready on file descriptor fd.
{
   if (array[fd]) array[fd]->InputReady();
}

void FDTable::OutputReady(int fd)	// Output ready on file descriptor fd.
{
   if (array[fd]) array[fd]->OutputReady();
}
