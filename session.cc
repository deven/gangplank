// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// session.cc -- Session class implementation.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Include files.
#include "phoenix.h"
#include "session.h"
#include "telnet.h"
#include "user.h"

Session::Session(Telnet *t)
{
   time_t now;				// current time

   telnet = t;				// Save Telnet pointer.
   next = NULL;				// No next session yet.

   name_only[0] = 0;			// No name yet.
   name[0] = 0;				// No name yet.
   blurb[0] = 0;			// No blurb yet.

   strcpy(default_sendlist, "everyone"); // Default sendlist is "everyone".
   last_sendlist[0] = 0;		// No previous sendlist yet.
   reply_sendlist[0] = 0;		// No reply sendlist yet.
   login_time = time(&now);		// Not logged in yet.
   idle_since = now;			// Reset idle time.

   user = new User(this);		// Create a new User for this Session.
}

Session::~Session()
{
   Session *s;
   int found;

   // Unlink session from list, remember if found.
   found = 0;
   if (sessions == this) {
      sessions = next;
      found++;
   } else {
      s = sessions;
      while (s && s->next != this) s = s->next;
      if (s && s->next == this) {
         s->next = next;
         found++;
      }
   }

   // Notify and log exit if session found.
   if (found) {
      notify("*** %s has left Phoenix! [%s] ***\n", name, date(0, 11, 5));
      log_message("Exit: %s (%s) on fd #%d.", name_only, user->user, telnet->fd);
   }

   delete user;
}

int Session::ResetIdle(int min)		// Reset/return idle time, maybe report.
{
   int now, idle, days, hours, minutes;

   now = time(NULL);
   idle = (now - idle_since) / 60;

   if (min && idle >= min) {
      hours = idle / 60;
      minutes = idle - hours * 60;
      days = hours / 24;
      hours -= days * 24;
      telnet->output("[You were idle for ");
      if (days) telnet->print("%d day%s%s ", days, days == 1 ? "" : "s",
                              hours ? "," : " and");
      if (hours) telnet->print("%d hour%s and ", hours, hours == 1 ? "" : "s");
      telnet->print("%d minute%s.]\n", minutes, minutes == 1 ? "" : "s");
   }
   idle_since = now;
   return idle;
}

// Send a message to everyone else signed on.
void Session::SendEveryone(const char *msg)
{
   int sent = 0;
   for (Session *session = sessions; session; session = session->next) {
      if (session == this) continue;
      session->telnet->PrintMessage(Public, name, name_only, NULL, msg);
      sent++;
   }

   switch (sent) {
   case 0:
      telnet->print("\a\aThere is no one else here! (message not sent)\n");
      break;
   case 1:
      ResetIdle(10);			// reset idle time
      telnet->print("(message sent to everyone.) [1 person]\n");
      break;
   default:
      ResetIdle(10);			// reset idle time
      telnet->print("(message sent to everyone.) [%d people]\n", sent);
      break;
   }
}

// Send private message by fd #.
void Session::SendByFD(int fd, const char *sendlist, int is_explicit,
                       const char *msg)
{
   // Save last sendlist if explicit.
   if (is_explicit && *sendlist) {
      strncpy(last_sendlist, sendlist, SendlistLen);
      last_sendlist[SendlistLen - 1] = 0;
   }

   for (Session *session = sessions; session; session = session->next) {
      if (session->telnet->fd == fd) {
         ResetIdle(10);			// reset idle time
         telnet->print("(message sent to %s.)\n", session->name);
         session->telnet->PrintMessage(Private, name, name_only, NULL, msg);
         return;
      }
   }
   telnet->print("\a\aThere is no user on fd #%d. (message not sent)\n", fd);
}

// Send private message by partial name match.
void Session::SendPrivate(const char *sendlist, int is_explicit,
                          const char *msg)
{
   // Save last sendlist if explicit.
   if (is_explicit && *sendlist) {
      strncpy(last_sendlist, sendlist, SendlistLen);
      last_sendlist[SendlistLen - 1] = 0;
   }

   if (!strcasecmp(sendlist, "me")) {
      ResetIdle(10);
      telnet->print("(message sent to %s.)\n", name);
      telnet->PrintMessage(Private, name, name_only, NULL, msg);
      return;
   }

   Session *session;
   Session *dest = NULL;
   int matches = 0;
   for (session = sessions; session; session = session->next) {
      if (match_name(session->name_only, sendlist)) {
         if (matches++) break;
         dest = session;
      }
   }

   switch (matches) {
   case 0:				// No matches.
      for (unsigned char *p = (unsigned char *) sendlist; *p; p++) {
         if (*p == UnquotedUnderscore) *p = Underscore;
      }
      telnet->print("\a\aNo names matched \"%s\". (message not sent)\n",
                    sendlist);
      break;
   case 1:				// Found single match, send message.
      ResetIdle(10);			// reset idle time
      telnet->print("(message sent to %s.)\n", dest->name);
      dest->telnet->PrintMessage(Private, name, name_only, NULL, msg);
      break;
   default:				// Multiple matches.
      telnet->print("\a\a\"%s\" matches %d names, including \"%s\" and \"%s\""
                    ". (message not sent)\n", sendlist, matches,
                    dest->name_only, session->name_only);
      break;
   }
}
