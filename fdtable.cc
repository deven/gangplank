// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// fdtable.cc -- FDTable class implementation.
//
// Copyright (c) 1992-1993 Deven T. Corzine
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
   array = new FD *[size];
   for (int i = 0; i < size; i++) array[i] = NULL;
}

FDTable::~FDTable()			// destructor
{
   for (int i = 0; i < used; i++) {
      if (array[i]) delete array[i];
   }
   delete array;
}

void FDTable::OpenListen(int port)	// Open a listening port.
{
   Listen *l = new Listen(port);
   if (l->fd < 0 || l->fd >= size) {
      error("FDTable::OpenListen(port = %d): fd #%d: range error! [0-%d]",
            port, l->fd, size - 1);
   }
   if (l->fd >= used) used = l->fd + 1;
   array[l->fd] = l;
   l->ReadSelect();
}

void FDTable::OpenTelnet(int lfd)	// Open a telnet connection.
{
   Telnet *t = new Telnet(lfd);
   if (t->fd < 0 || t->fd >= size) {
      warn("FDTable::OpenTelnet(lfd = %d): fd #%d: range error! [0-%d]", lfd,
           t->fd, size - 1);
      delete t;
      return;
   }
   if (t->fd >= used) used = t->fd + 1;
   array[t->fd] = t;
}

FD *FDTable::Closed(int fd)		// Close fd, return FD object pointer.
{
   if (fd < 0 || fd >= used) {
      error("FDTable::Closed(fd = %d): range error! [0-%d]", fd, used - 1);
   }
   FD *FD = array[fd];
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
   delete Closed(fd);
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
   if (fd < 0 || fd >= used) {
      error("FDTable::InputReady(fd = %d): range error! [0-%d]", fd, used - 1);
   }
   array[fd]->InputReady(fd);
}

void FDTable::OutputReady(int fd)	// Output ready on file descriptor fd.
{
   if (fd < 0 || fd >= used) {
      error("FDTable::OutputReady(fd = %d): range error! [0-%d]", fd,
            used - 1);
   }
   array[fd]->OutputReady(fd);
}

// Unformatted write to all connections.
void FDTable::announce(const char *buf)
{
   Telnet *t;

   for (int i = 0; i < used; i++) {
      if ((t = (Telnet *) array[i]) && t->type == TelnetFD) {
         t->UndrawInput();
         t->output(buf);
         t->RedrawInput();
      }
   }
}

// Nuke a user (force close connection).
void FDTable::nuke(Telnet *telnet, int fd, bool drain)
{
   Telnet *t;

   if (fd >= 0 && fd < used && (t = (Telnet *) array[fd]) &&
       t->type == TelnetFD) {
      t->UndrawInput();
      t->print("\a\a\a*** You have been nuked by %s. ***\n",
               telnet->session->name);
      log_message("%s (%s) on fd #%d has been nuked by %s (%s).",
                  t->session->name, t->session->user->user, fd,
                  telnet->session->name, telnet->session->user->user);
      t->nuke(telnet, drain);
   } else {
      telnet->print("There is no user on fd #%d.\n", fd);
   }
}
