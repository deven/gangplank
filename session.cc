// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// session.cc -- Session class implementation.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Include files.
#include "phoenix.h"
#include "session.h"
#include "telnet.h"
#include "user.h"

Session::Session(Telnet *t)
{
   time_t now;				// current time

   telnet = t;				// Save Telnet pointer.
   next = NULL;				// No next session yet.
   name[0] = 0;				// No name yet.

   strcpy(default_sendlist, "everyone"); // Default sendlist is "everyone".
   last_sendlist[0] = 0;		// No previous sendlist yet.
   login_time = time(&now);		// Not logged in yet.
   idle_since = now;			// Reset idle time.

   user = new User(this);		// Create a new User for this Session.
}

Session::~Session()
{
   Session *s;
   int found;

   // Unlink session from list, remember if found.
   found = 0;
   if (sessions == this) {
      sessions = next;
      found++;
   } else {
      s = sessions;
      while (s && s->next != this) s = s->next;
      if (s && s->next == this) {
         s->next = next;
         found++;
      }
   }

   // Notify and log exit if session found.
   if (found) {
      notify("*** %s has left Phoenix! [%s] ***\n", name, date(0, 11, 5));
      log_message("Exit: %s (%s) on fd #%d.", name, user->user, telnet->fd);
   }

   delete user;
}
