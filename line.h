// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// line.h -- Line class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _LINE_H
#define _LINE_H 1

// Include files.
#include "object.h"
#include "phoenix.h"

// Single input lines waiting to be processed.
class Line: public Object {
public:
   const char *line;			// input line
   Pointer<Line> next;			// next input line

   Line(const char *p) {		// constructor
      line = new char[strlen(p) + 1];
      strcpy((char *) line, p);
      next = NULL;
   }
   ~Line() {				// destructor
      delete line;
   }
   void Append(Line *p) {		// Add new line at end of list.
      if (next) {
         next->Append(p);
      } else {
         next = p;
      }
   }
};

#endif // line.h
