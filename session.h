// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// session.h -- Session class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _SESSION_H
#define _SESSION_H 1

// Include files.
#include "object.h"
#include "outbuf.h"
#include "output.h"
#include "outstr.h"
#include "phoenix.h"

// Data about a particular session.
class Session: public Object {
protected:
   static Pointer<Session> sessions;	// List of all sessions. (global)
public:
   Pointer<Session> next;		// next session
   Pointer<User> user;			// user this session belongs to
   Pointer<Telnet> telnet;		// telnet connection for this session
   InputFuncPtr InputFunc;		// function pointer for input processor
   Pointer<Line> lines;			// unprocessed input lines
   OutputBuffer OutBuf;			// temporary output buffer
   OutputStream Pending;		// pending output stream
   time_t login_time;			// time logged in
   time_t idle_since;			// last idle time
   bool SignalPublic;			// Signal for public messages?
   bool SignalPrivate;			// Signal for private messages?
   bool closing;			// Session closing?
   char name_only[NameLen];		// current user name (pseudo) alone
   char name[NameLen];			// current user name (pseudo) with blurb
   char blurb[NameLen];			// current user blurb
   Pointer<Name> name_obj;		// current name object.
   char default_sendlist[SendlistLen];	// current default sendlist
   char last_sendlist[SendlistLen];	// last explicit sendlist
   char reply_sendlist[SendlistLen];	// reply sendlist for last sender

   Session(Telnet *t);			// constructor
   ~Session();				// destructor
   void Close(bool drain = true);	// Close session.
   void SaveInputLine(const char *line);
   void SetInputFunction(InputFuncPtr input);
   void InitInputFunction();
   void Input(const char *line);

   void output(int byte) {		// queue output byte
      OutBuf.out(byte);
   }
   void output(const char *buf) {	// queue output data
      if (!buf) return;			// return if no data
      while (*buf) OutBuf.out(*((unsigned char *) buf++));
   }
   void print(const char *format, ...);	// formatted output

   void EnqueueOutput(void) {		// Enqueue output buffer.
      char *buf = OutBuf.GetData();
      if (buf) Pending.Enqueue(telnet, new Text(buf));
   }
   void Enqueue(Output *out) {		// Enqueue output buffer and object.
      EnqueueOutput();
      Pending.Enqueue(telnet, out);
   }
   void EnqueueOthers(Output *out) {	// Enqueue output to others.
      Session *session;
      for (session = sessions; session; session = session->next) {
         if (session == this) continue;
         session->Enqueue(out);
      }
   }
   void AcknowledgeOutput(void) {	// Output acknowledgement.
      Pending.Acknowledge();
   }
   bool OutputNext(Telnet *telnet) {	// Output next output block.
      return Pending.SendNext(telnet);
   }

   void Login(const char *line);	// Process response to login prompt.
   void Password(const char *line);	// Process response to password prompt.
   void DoName(const char *line);	// Process response to name prompt.
   void Blurb(const char *line);	// Process response to blurb prompt.
   void ProcessInput(const char *line);	// Process normal input.
   void NotifyEntry();			// Notify other users of entry and log.
   void NotifyExit();			// Notify other users of exit and log.
   int ResetIdle(int min);		// Reset/return idle time, maybe report.
   void DoDown(const char *args);	// Do !down command.
   void DoNuke(const char *args);	// Do !nuke command.
   void DoBye();			// Do /bye command.
   void DoDetach();			// Do /detach command.
   void DoWho();			// Do /who command.
   void DoIdle();			// Do /idle command.
   void DoDate();			// Do /date command.
   void DoSignal(const char *p);	// Do /signal command.
   void DoSend(const char *p);		// Do /send command.
   void DoWhy();			// Do /why command.
   int DoBlurb(const char *start, bool entry = false); // Do /blurb command.
   void DoHelp();			// Do /help command.
   void DoReset();			// Do <space><return> idle time reset.
   void DoMessage(const char *line);	// Do message send.

   // Send public message to everyone.
   void SendEveryone(const char *msg);

   // Send private message by fd #.
   void SendByFD(int fd, const char *msg);

   // Send private message by partial name match.
   void SendPrivate(const char *sendlist, const char *msg);

   // Exit if shutting down and no users are left.
   static void CheckShutdown();
};

#endif // session.h
