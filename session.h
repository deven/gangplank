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

   Session(Telnet *t);			// constructor
   ~Session();				// destructor
   int ResetIdle(int min);		// Reset/return idle time, maybe report.
};

#endif // session.h
