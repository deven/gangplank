// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// user.h -- User class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _USER_H
#define _USER_H 1

// Include files.
#include "object.h"
#include "phoenix.h"

// Data about a particular user.
class User: public Object {
public:
   int priv;				// privilege level
   // XXX change! vvv
   char user[32];			// account name
   char password[32];			// password for this account
   // XXX change! ^^^
   char reserved_name[NameLen];		// reserved user name (pseudo)
   char default_blurb[NameLen];		// default blurb

   User(Session *s);			// constructor
};

#endif // user.h
