// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// output.h -- Output and derived classes, interfaces.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _OUTPUT_H
#define _OUTPUT_H 1

// Include files.
#include "name.h"
#include "object.h"
#include "phoenix.h"

// Types of Output subclasses.
enum OutputType {
   UnknownOutput, TextOutput, PublicMessage, PrivateMessage, EntryOutput,
   ExitOutput, AttachOutput, DetachOutput
};

// Classifications of Output subclasses.
enum OutputClass {UnknownClass, TextClass, MessageClass, NotificationClass};

class Output: public Object {
public:
   OutputType Type;			// Output type.
   OutputClass Class;			// Output class.
   time_t time;				// Timestamp.

   // constructor
   Output(OutputType t, OutputClass c, time_t when = 0): Type(t), Class(c) {
      if (when) {
         time = when;
      } else {
         ::time(&time);
      }
   }
   virtual ~Output() {}			// destructor
   virtual void output(Telnet *telnet) = 0;
};

class Text: public Output {
protected:
   const char *text;
public:
   Text(const char *buf): Output(TextOutput, TextClass), text(buf) { }
   ~Text() { delete[] text; }
   void output(Telnet *telnet);
};

class Message: public Output {
protected:
   Pointer<Name> from;
   // Pointer<Sendlist> to;
   const char *text;
public:
   Message(OutputType type, Name *sender, const char *msg):
      Output(type, MessageClass), from(sender) {
      text = new char[strlen(msg) + 1];
      strcpy((char *) text, msg);
   }
   ~Message() { delete text; }
   void output(Telnet *telnet);
};

class EntryNotify: public Output {
protected:
   Pointer<Name> name;
public:
   EntryNotify(Name *who, time_t when = 0):
      Output(EntryOutput, NotificationClass, when), name(who) { }
   void output(Telnet *telnet);
};

class ExitNotify: public Output {
protected:
   Pointer<Name> name;
public:
   ExitNotify(Name *who, time_t when = 0):
      Output(ExitOutput, NotificationClass, when), name(who) { }
   void output(Telnet *telnet);
};

class AttachNotify: public Output {
private:
   Pointer<Name> name;
public:
   AttachNotify(Name *who, time_t when = 0):
      Output(AttachOutput, NotificationClass, when), name(who) { }
   void output(Telnet *telnet);
};

class DetachNotify: public Output {
private:
   Pointer<Name> name;
   bool intentional;
public:
   DetachNotify(Name *who, bool i, time_t when = 0):
      Output(DetachOutput, NotificationClass, when), name(who), intentional(i) {
   }
   void output(Telnet *telnet);
};

#endif // output.h
