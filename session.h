// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// session.h -- Session class interface.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _SESSION_H
#define _SESSION_H 1

// Include files.
#include "phoenix.h"

// Data about a particular session.
class Session {
protected:
   static Session *sessions;		// List of all sessions. (global)
public:
   Session *next;			// next session
   User *user;				// user this session belongs to
   Telnet *telnet;			// telnet connection for this session
   time_t login_time;			// time logged in
   time_t idle_since;			// last idle time
   char name_only[NameLen];		// current user name (pseudo) alone
   char name[NameLen];			// current user name (pseudo) with blurb
   char blurb[NameLen];			// current user blurb
   char default_sendlist[SendlistLen];	// current default sendlist
   char last_sendlist[SendlistLen];	// last explicit sendlist
   char reply_sendlist[SendlistLen];	// reply sendlist for last sender

   Session(Telnet *t);			// constructor
   ~Session();				// destructor
   void Link();				// Link session into global list.
   int ResetIdle(int min);		// Reset/return idle time, maybe report.

   // Send public message to everyone.
   void SendEveryone(const char *msg);

   // Send private message by fd #.
   void SendByFD(int fd, const char *sendlist, int is_explicit,
                 const char *msg);

   // Send private message by partial name match.
   void SendPrivate(const char *sendlist, int is_explicit, const char *msg);

   // Formatted write to all sessions.
   static void notify(const char *format, ...);

   // Run /who command.
   static void who_cmd(Telnet *telnet);

   // Exit if shutting down and no users are left.
   static void CheckShutdown();
};

#endif // session.h
