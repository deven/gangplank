// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// name.h -- Name class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _NAME_H
#define _NAME_H 1

// Include files.
#include "object.h"
#include "phoenix.h"

class Name: public Object {
public:
   Pointer<Name> next;			// Next name used by this session.
   Pointer<Session> session;		// Session this name refers to.
   char name[NameLen];			// Name string.

   // constructor
   Name(Session *s, Name *prev, char *str): session(s) {
      // Delete leading unused names. (may not work)
      while (prev && prev->References() == 1) prev = prev->next;
      strncpy(name, str, NameLen);	// Save name string.
      name[NameLen - 1] = 0;		// Make sure name is terminated.
      next = prev;			// Save pointer to previous name used.
   }
};

#endif // name.h
