// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// output.h -- Output and derived classes, interfaces.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _OUTPUT_H
#define _OUTPUT_H 1

// Include files.
#include "name.h"
#include "phoenix.h"

// Types of Output subclasses.
enum OutputType {
   UnknownOutput, TextOutput, PublicMessage, PrivateMessage, EntryOutput,
   ExitOutput
};

// Classifications of Output subclasses.
enum OutputClass {UnknownClass, TextClass, MessageClass, NotificationClass};

class Output {
public:
   OutputType Type;			// Output type.
   OutputClass Class;			// Output class.
   time_t time;				// Timestamp.
   int RefCnt;				// Reference count.

   Output(time_t when = 0) {		// constructor
      if (when) {
         time = when;
      } else {
         ::time(&time);
      }
      RefCnt = 0;
   }
   virtual ~Output() {}			// destructor
   virtual void output(Telnet *telnet) = 0;
};

class Text: public Output {
   char *text;
public:
   Text(char *buf): Output() {
      Type = TextOutput;
      Class = TextClass;
      text = buf;
   }
   ~Text() {
      delete text;
   }
   void output(Telnet *telnet);
};

class Message: public Output {
public:
   Name *from;
   // Sendlist *to; XXX
   char *text;
   Message(OutputType type, Name *sender, const char *msg): Output() {
      Type = type;
      Class = MessageClass;
      if ((from = sender)) from->RefCnt++;
      text = new char[strlen(msg) + 1];
      strcpy(text, msg);
   }
   ~Message() {
      if (from && --from->RefCnt == 0) delete from;
      delete text;
   }
   void output(Telnet *telnet);
};

class EntryNotify: public Output {
public:
   Name *name;
   EntryNotify(Name *name_obj, time_t when = 0): Output(when) {
      Type = EntryOutput;
      Class = NotificationClass;
      if ((name = name_obj)) name->RefCnt++;
   }
   ~EntryNotify() {			// destructor
      if (name && --name->RefCnt == 0) delete name;
   }
   void output(Telnet *telnet);
};

class ExitNotify: public Output {
public:
   Name *name;
   ExitNotify(Name *name_obj, time_t when = 0): Output(when) { // constructor
      Type = ExitOutput;
      Class = NotificationClass;
      if ((name = name_obj)) name->RefCnt++;
   }
   ~ExitNotify() {			// destructor
      if (name && --name->RefCnt == 0) delete name;
   }
   void output(Telnet *telnet);
};

#endif // output.h