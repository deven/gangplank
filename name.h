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
#include "phoenix.h"

class Name {
public:
   Name *next;				// Next name used by this session.
   Session *session;			// Session this name refers to.
   char name[NameLen];			// Name string.
   int RefCnt;				// Reference count.

   Name(Session *s, Name *prev, char *str) { // constructor
      session = s;			// Save session pointer.
      next = prev;			// Save previous name used.
      while (next && next->RefCnt == 1) { // Delete leading unused names.
         prev = next->next;		// Save next name pointer.
         delete next;			// Delete name object.
         next = prev;			// Save new previous name used.
      }
      strncpy(name, str, NameLen);	// Save name string.
      name[NameLen - 1] = 0;		// Make sure name is terminated.
      RefCnt = 1;			// Set reference count to one.
   }
};

#endif // name.h
