// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// user.cc -- User class implementation.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Include files.
#include "phoenix.h"
#include "user.h"

User::User(Session *s)
{
   priv = 10;				// default user privilege level
   strcpy(user, "[nobody]");		// Who is this?
   password[0] = 0;			// No password.
   reserved_name[0] = 0;		// No name.
}
