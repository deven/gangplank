// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// session.cc -- Session class implementation.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Include files.
#include "line.h"
#include "phoenix.h"
#include "session.h"
#include "telnet.h"
#include "user.h"

Pointer<Session> Session::sessions = NULL;

Session::Session(Telnet *t)
{
   time_t now;				// current time

   next = NULL;				// No next session.
   user = new User(this);		// XXX Create a User for this Session.
   telnet = t;				// Save Telnet pointer.
   login_time = time(&now);		// Not logged in yet.
   idle_since = now;			// Reset idle time.
   name_only[0] = 0;			// No name.
   name[0] = 0;				// No name/blurb.
   blurb[0] = 0;			// No blurb.
   strcpy(default_sendlist, "everyone"); // Default sendlist is "everyone".
   last_sendlist[0] = 0;		// No previous sendlist yet.
   reply_sendlist[0] = 0;		// No reply sendlist yet.

   InputFunc = NULL;			// No input function.
   lines = NULL;			// No pending input lines.
   name_obj = NULL;			// No name object.
   SignalPublic = true;			// Default public signal on. (for now)
   SignalPrivate = true;		// Default private signal on.
   SignedOn = false;			// No signed on yet.
}

Session::~Session()
{
   Close();
}

void Session::Close(bool drain)		// Close session.
{
   // Unlink session from list.
   Pointer<Session> s = sessions;
   if (sessions == this) {
      sessions = next;
   } else {
      while (s && s->next != this) s = s->next;
      if (s && s->next == this) {
         s->next = next;
      }
   }

   if (SignedOn) NotifyExit();		// Notify and log exit if signed on.

   SignedOn = false;

   if (telnet) {
      Pointer<Telnet> t = telnet;
      telnet = NULL;
      t->Close(drain);			// Close connection.
   }

   user = NULL;
}

void Session::Attach(Telnet *t)		// Attach session to telnet connection.
{
   if (t) {
      telnet = t;
      telnet->session = this;
      log_message("Attach: %s (%s) on fd #%d.", name_only, user->user, telnet->fd);
      EnqueueOthers(new AttachNotify(name_obj));
      Pending.Attach(telnet);
      output("*** End of reviewed output. ***\n");
      EnqueueOutput();
   }
}

void Session::Detach(bool intentional)	// Detach session from connection.
{
   if (SignedOn && telnet) {
      if (intentional) {
         log_message("Detach: %s (%s) on fd #%d. (intentional)", name_only, user->user,
             telnet->fd);
      } else {
         log_message("Detach: %s (%s) on fd #%d. (accidental)", name_only, user->user,
             telnet->fd);
      }
      EnqueueOthers(new DetachNotify(name_obj, intentional));
      telnet = NULL;
   } else {
      Close();
   }
}

void Session::SaveInputLine(const char *line)
{
   Line *p;

   p = new Line(line);
   if (lines) {
      lines->Append(p);
   } else {
      lines = p;
   }
}

void Session::SetInputFunction(InputFuncPtr input)
{
   InputFunc = input;

   // Process lines as long as we still have a defined input function.
   while (InputFunc != NULL && lines) {
      (this->*InputFunc)(lines->line);
      lines = lines->next;
      EnqueueOutput();			// Enqueue output buffer (if any).
   }
}

void Session::InitInputFunction()	// Initialize input function to Login.
{
   SetInputFunction(&Session::Login);
}

void Session::Input(const char *line)	// Process an input line.
{
   Pending.Dequeue();			// Dequeue all acknowledged output.
   if (InputFunc) {			// If available, call immediately.
      (this->*InputFunc)(line);
      EnqueueOutput();			// Enqueue output buffer (if any).
   } else {				// Otherwise, save input line for later.
      SaveInputLine(line);
   }
}

void Session::print(const char *format, ...) // formatted write
{
   char buf[BufSize];
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   output(buf);
}

void Session::announce(const char *format, ...) // print to all sessions
{
   Session *session;
   char buf[BufSize];
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   for (session = sessions; session; session = session->next) {
      session->output(buf);
      session->EnqueueOutput();
   }
}

void Session::Login(const char *line)	// Process response to login prompt.
{
   if (!strcasecmp(line, "/bye")) {
      DoBye();
      return;
//   } else if (!strcasecmp(line, "/who")) {
//      DoWho();
//      telnet->Prompt("login: ");
//      return;
//   } else if (!strcasecmp(line, "/idle")) {
//      DoIdle();
//      telnet->Prompt("login: ");
//      return;
   } else if (!strcasecmp(line, "guest")) {
      strcpy(user->user, line);
      name[0] = 0;
      user->priv = 0;
      telnet->output(Newline);
      telnet->Prompt("Enter name: ");	// Prompt for name.
      SetInputFunction(&Session::DoName); // Set name input routine.
      return;
   } else {
      int found = 0;
      char buf[256], *username, *passwd, *name, *priv, *p;
      FILE *pw = fopen("passwd", "r");
      if (pw) {
         while (fgets(buf, 256, pw)) {
            if (buf[0] == '#') continue;
            p = username = buf;
            passwd = name = priv = 0;
            while (*p) if (*p == ':') { *p++ = 0; passwd = p; break; } else p++;
            while (*p) if (*p == ':') { *p++ = 0; name = p; break; } else p++;
            while (*p) if (*p == ':') { *p++ = 0; priv = p; break; } else p++;
            if (!priv) continue;
            if (!strcasecmp(line, username)) {
               found = 1;
               strcpy(user->user, username);
               strcpy(user->password, passwd);
               strcpy(name_only, name);
               user->priv = atoi(priv ? priv : "0");
               break;
            }
         }
      }
      fclose(pw);
      if (!found) {
         if (*line) telnet->output("Login incorrect.\n");
         telnet->Prompt("login: ");
         return;
      }
   }

   // Warn if echo can't be turned off.
   if (!telnet->Echo) {
      telnet->output("\n\aSorry, password WILL echo.\n\n");
   } else if (telnet->Echo != TelnetEnabled) {
      telnet->output("\nWarning: password may echo.\n\n");
   }

   telnet->Prompt("Password: ");	// Prompt for password.
   telnet->DoEcho = false;		// Disable echoing.
   SetInputFunction(&Session::Password); // Set password input routine.
}

void Session::Password(const char *line) // Process response to password prompt.
{
   telnet->output(Newline);		// Send newline.
   telnet->DoEcho = true;		// Enable echoing.

   // Check against encrypted password.
   if (strcmp(crypt(line, user->password), user->password)) {
      telnet->output("Login incorrect.\n");
      telnet->Prompt("login: ");	// Prompt for login.
      SetInputFunction(&Session::Login); // Set login input routine.
      return;
   }

   telnet->print("\nYour default name is \"%s\".\n\n", name_only);
   telnet->Prompt("Enter name: ");	// Prompt for name.
   SetInputFunction(&Session::DoName);	// Set name input routine.
}

void Session::DoName(const char *line)	// Process response to name prompt.
{
   if (!*line) {			// blank line
      if (!strcasecmp(user->user, "guest")) {
         telnet->output(Newline);
         telnet->Prompt("Enter name: ");
         return;
      }
   } else {
      strncpy(name_only, line, NameLen); // Save user's name.
      name_only[NameLen - 1] = 0;
   }
   Session *session;
   for (session = sessions; session; session = session->next) {
      if (!strcasecmp(session->name_only, name_only)) {
         if (!strcmp(session->user->user, user->user) && !session->telnet) {
            telnet->output("Re-attaching to detached session...\n");
            session->Attach(telnet);
            telnet = NULL;
            Close();
            return;
         } else {
            telnet->output("That name is already in use.  Choose another.\n");
            telnet->Prompt("Enter name: ");
            return;
         }
      }
   }
   telnet->Prompt("Enter blurb: ");	// Prompt for blurb.
   SetInputFunction(&Session::Blurb);	// Set blurb input routine.
}

void Session::Blurb(const char *line)	// Process response to blurb prompt.
{
   if (!line || !*line) line = user->default_blurb;
   int over = DoBlurb(line, true);
   if (over) {
      telnet->print("The combination of your name and blurb is %d "
                    "character%s too long.\n", over, over == 1 ? "" : "s");
      telnet->Prompt("Enter blurb: ");	// Prompt for blurb.
      return;
   }

   SignedOn = true;			// Session is signed on.

   NotifyEntry();			// Notify other users of entry.

   // Print welcome banner and do a /who list.
   output("\n\nWelcome to Phoenix.  Type \"/help\" for a list of commands."
          "\n\n");
   DoWho();				// Enqueues output.

   SetInputFunction(&Session::ProcessInput); // Set normal input routine.
}

void Session::ProcessInput(const char *line) // Process normal input.
{
   // XXX Make ! normal for average users?  normal if not a valid command?
   if (*line == '!') {
      // XXX add !priv command?
      // XXX do individual privilege levels for each !command?
      if (user->priv < 50) {
         output("Sorry, all !commands are privileged.\n");
         return;
      }
      if (!strncasecmp(line, "!down", 5)) {
         while (*line && !isspace(*line)) line++;
         while (*line && isspace(*line)) line++;
         DoDown(line);
      } else if (!strncasecmp(line, "!nuke ", 6)) {
         while (*line && !isspace(*line)) line++;
         while (*line && isspace(*line)) line++;
         DoNuke(line);
      } else {
         // Unknown !command.
         output("Unknown !command.\n");
      }
   } else if (*line == '/') {
      if (!strncasecmp(line, "/bye", 4)) {
         DoBye();
      } else if (!strncasecmp(line, "/clear", 6)) {
         DoClear();
      } else if (!strncasecmp(line, "/unidle", 7)) {
         DoReset();
      } else if (!strncasecmp(line, "/detach", 4)) {
         DoDetach();
      } else if (!strncasecmp(line, "/who", 4)) {
         DoWho();
      } else if (!strncasecmp(line, "/idle", 3)) {
         DoIdle();
      } else if (!strcasecmp(line, "/date")) {
         DoDate();
      } else if (!strncasecmp(line, "/signal", 7)) {
         DoSignal(line + 7);
      } else if (!strncasecmp(line, "/send", 5)) {
         DoSend(line + 5);
      } else if (!strncasecmp(line, "/why", 4)) {
         DoWhy();
      } else if (!strncasecmp(line, "/blurb", 3)) { // /blurb command.
         while (*line && !isspace(*line)) line++;
         DoBlurb(line);
      } else if (!strncasecmp(line, "/help", 5)) { // /help command.
         DoHelp();
      } else {				// Unknown /command.
         output("Unknown /command.  Type /help for help.\n");
      }
   } else if (!strcmp(line, " ")) {
      DoReset();
   } else if (*line) {
      DoMessage(line);
   }
}

void Session::NotifyEntry()		// Notify other users of entry and log.
{
   log_message("Enter: %s (%s) on fd #%d.", name_only, user->user, telnet->fd);
   EnqueueOthers(new EntryNotify(name_obj, idle_since = time(&login_time)));
   next = sessions;			// Link session into global list.
   sessions = this;
   // XXX Link new session into user list.
}

void Session::NotifyExit()		// Notify other users of exit and log.
{
   if (telnet) {
      log_message("Exit: %s (%s) on fd #%d.", name_only, user->user,
                  telnet->fd);
   } else {
      log_message("Exit: %s (%s), detached.", name_only, user->user);
   }
   EnqueueOthers(new ExitNotify(name_obj));
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
      output("[You were idle for");
      if (!minutes) output(" exactly");
      if (days) print(" %d day%s%s", days, days == 1 ? "" : "s", hours &&
                      minutes ? "," : " and");
      if (hours) print(" %d hour%s%s", hours, hours == 1 ? "" : "s", minutes ?
                       " and" : "");
      if (minutes) print(" %d minute%s", minutes, minutes == 1 ? "" : "s");
      output(".]\n");
   }
   idle_since = now;
   return idle;
}

void Session::DoDown(const char *args)	// Do !down command.
{
   if (!strcmp(args, "!")) {
      log_message("Immediate shutdown requested by %s (%s).", name_only,
                  user->user);
      log_message("Final shutdown warning.");
      announce("*** %s has shut down Phoenix! ***\n", name);
      announce("\a\a>>> Server shutting down NOW!  Goodbye. <<<\n\a\a");
      alarm(5);
      Shutdown = 2;
   } else if (!strcasecmp(args, "cancel")) {
      if (Shutdown) {
         Shutdown = 0;
         alarm(0);
         log_message("Shutdown cancelled by %s (%s).", name_only, user->user);
         announce("*** %s has cancelled the server shutdown. ***\n", name);
      } else {
         output("The server was not about to shut down.\n");
      }
   } else {
      int seconds;
      if (sscanf(args, "%d", &seconds) != 1) seconds = 30;
      log_message("Shutdown requested by %s (%s) in %d seconds.", name_only,
                  user->user, seconds);
      announce("*** %s has shut down Phoenix! ***\n", name);
      announce("\a\a>>> This server will shutdown in %d seconds... <<<\n\a\a",
               seconds);
      alarm(seconds);
      Shutdown = 1;
   }
}

void Session::DoNuke(const char *args)	// Do !nuke command.
{
   bool drain;
   Session *session, *target, *extra;
   Telnet *telnet;
   int matches = 0;

   if (!(drain = bool(*args != '!'))) args++;

   if (!strcasecmp(args, "me")) {
      target = this;
   } else {
      for (session = sessions; session; session = session->next) {
         if (strcasecmp(session->name_only, args)) {
            if (match_name(session->name_only, args)) {
               if (matches++) {
                  extra = session;
               } else {
                  target = session;
               }
            }
         } else {			// Found exact match; use it.
            target = session;
            matches = 1;
            break;
         }
      }

      // XXX kludge
      for (unsigned char *p = (unsigned char *) args; *p; p++) {
         if (*p == UnquotedUnderscore) *p = Underscore;
      }

      switch (matches) {
      case 0:				// No matches.
         print("\a\aNo names matched \"%s\". (nobody nuked)\n", args);
         return;
      case 1:				// Found single match, nuke session.
         break;
      default:				// Multiple matches.
         print("\a\a\"%s\" matches %d names, including \"%s\" and \"%s\". "
               "(nobody nuked)\n", args, matches, target->name_only,
               extra->name_only);
         return;
      }
   }

   // Nuke target session.		// XXX Should require confirmation!
   if (drain) {
      print("\"%s\" has been nuked.\n", target->name_only);
   } else {
      print("\"%s\" has been nuked immediately.\n", target->name_only);
   }

   if (target->telnet) {
      telnet = target->telnet;
      target->telnet = NULL;
      log_message("%s (%s) on fd #%d has been nuked by %s (%s).",
                  target->name_only, target->user->user, telnet->fd, name_only,
                  user->user);
      telnet->UndrawInput();
      telnet->print("\a\a\a*** You have been nuked by %s. ***\n", name);
      telnet->RedrawInput();
      telnet->Close(drain);
   } else {
      log_message("%s (%s), detached, has been nuked by %s (%s).",
                  target->name_only, target->user->user, name_only, user->user);
      target->Close();
   }
}

void Session::DoBye()			// Do /bye command.
{
   Close();				// Close session.
}

void Session::DoClear()			// Do /clear command.
{
   output("\033[H\033[J");		// XXX ANSI!
}

void Session::DoDetach()		// Do /detach command.
{
   output("You have been detached.\n");
   EnqueueOutput();
   if (telnet) telnet->Close();		// Drain connection, then close.
}

void Session::DoWho()			// Do /who command.
{
   int idle, days, hours, minutes;
   int now = time(NULL);

   // Check if anyone is signed on at all.
   if (!sessions) {
      output("Nobody is signed on.\n");
      return;
   }

   // Output /who header.
   output("\n"
          " Name                              On Since   Idle  User\n"
          " ----                              --------   ----  ----\n");

   // Output data about each user.
   Session *session;
   for (session = sessions; session; session = session->next) {
      if (session->telnet) {
         output(Space);
      } else {
         output(Tilde);
      }
      print("%-32s  ", session->name);
      if (session->telnet) {
         if ((now - session->login_time) < 86400) {
            output(date(session->login_time, 11, 8));
         } else {
            output(Space);
            output(date(session->login_time, 4, 6));
            output(Space);
         }
      } else {
         output("detached");
      }
      idle = (now - session->idle_since) / 60;
      if (idle) {
         hours = idle / 60;
         minutes = idle - hours * 60;
         days = hours / 24;
         hours -= days * 24;
         if (days > 9 || (days && !session->telnet)) {
            print("%2dd%02d:%02d ", days, hours, minutes);
         } else if (days) {
            print("%dd%02d:%02d  ", days, hours, minutes);
         } else if (hours) {
            print("  %2d:%02d  ", hours, minutes);
         } else {
            print("     %2d  ", minutes);
         }
      } else {
         output("         ");
      }
      output(session->user->user);
      output(Newline);
   }
}

void Session::DoIdle()			// Do /idle command.
{
   int idle, days, hours, minutes;
   int now = time(NULL);
   int col = 0;

   // Check if anyone is signed on at all.
   if (!sessions) {
      output("Nobody is signed on.\n");
      return;
   }

   // Output /idle header.
   if (sessions && !sessions->next) {
      // XXX get LISTED user count better.
      output("\n Name                              Idle\n ----              "
             "                ----\n");
   } else {
      output("\n Name                              Idle  Name               "
             "               Idle\n ----                              ----  "
             "----                              ----\n");
   }

   // Output data about each user.
   Session *session;
   for (session = sessions; session; session = session->next) {
      if (session->telnet) {
         output(Space);
      } else {
         output(Tilde);
      }
      print("%-32s ", session->name);
      idle = (now - session->idle_since) / 60;
      if (idle) {
         hours = idle / 60;
         minutes = idle - hours * 60;
         days = hours / 24;
         hours -= days * 24;
         if (days > 9) {
            print("%2dd%02d", days, hours);
         } else if (days) {
            print("%dd%02dh", days, hours);
         } else if (hours) {
            print("%2d:%02d", hours, minutes);
         } else {
            print("   %2d", minutes);
         }
      } else {
         output("     ");
      }
      output(col ? Newline : Space);
      col = !col;
   }
   if (col) output(Newline);
}

void Session::DoDate()			// Do /date command.
{
   print("%s\n", date(0, 0, 0));	// Print current date and time.
}

void Session::DoSignal(const char *p)	// Do /signal command.
{
   while (*p && isspace(*p)) p++;
   if (!strncasecmp(p, "on", 2)) {
      SignalPublic = SignalPrivate = true;
      output("All signals are now on.\n");
   } else if (!strncasecmp(p, "off", 3)) {
      SignalPublic = SignalPrivate = false;
      output("All signals are now off.\n");
   } else if (!strncasecmp(p, "public", 6)) {
      p += 6;
      while (*p && isspace(*p)) p++;
      if (!strncasecmp(p, "on", 2)) {
         SignalPublic = true;
         output("Signals for public messages are now on.\n");
      } else if (!strncasecmp(p, "off", 3)) {
         SignalPublic = false;
         output("Signals for public messages are now off.\n");
      } else {
         output("/signal public syntax error!\n");
      }
   } else if (!strncasecmp(p, "private", 7)) {
      p += 7;
      while (*p && isspace(*p)) p++;
      if (!strncasecmp(p, "on", 2)) {
         SignalPrivate = true;
         output("Signals for private messages are now on.\n");
      } else if (!strncasecmp(p, "off", 3)) {
         SignalPrivate = false;
         output("Signals for private messages are now off.\n");
      } else {
         output("/signal private syntax error!\n");
      }
   } else {
      output("/signal syntax error!\n");
   }
}

void Session::DoSend(const char *p)	// Do /send command.
{
   while (*p && isspace(*p)) p++;
   if (!*p) {				// Display current sendlist.
      if (!default_sendlist[0]) {
         output("Your default sendlist is turned off.\n");
      } else if (!strcasecmp(default_sendlist, "everyone")) {
         output("You are sending to everyone.\n");
      } else {
         print("Your default sendlist is set to \"%s\".\n", default_sendlist);
      }
   } else if (!strcasecmp(p, "off")) {
      default_sendlist[0] = 0;
      output("Your default sendlist has been turned off.\n");
   } else if (!strcasecmp(p, "everyone")) {
      strcpy(default_sendlist, p);
      output("You are now sending to everyone.\n");
   } else {
      strncpy(default_sendlist, p, SendlistLen);
      default_sendlist[SendlistLen - 1] = 0;
      print("Your default sendlist is now set to \"%s\".\n", default_sendlist);
   }
}

void Session::DoWhy()			// Do /why command.
{
   output("Why not?\n");
}

// Do /blurb command (or blurb set on entry), return number of bytes truncated.
int Session::DoBlurb(const char *start, bool entry)
{
   const char *end;
   while (*start && isspace(*start)) start++;
   if (*start) {
      for (const char *p = start; *p; p++) if (!isspace(*p)) end = p;
      if (strncasecmp(start, "off", end - start + 1)) {
         if ((*start == '\"' && *end == '\"' && start < end) ||
             (*start == '[' && *end == ']')) start++; else end++;
         int len = end - start;
         int over = len - (NameLen - strlen(name_only) - 4);
         if (over < 0) over = 0;
         len -= over;
         strncpy(blurb, start, len);
         blurb[len] = 0;
         sprintf(name, "%s [%s]", name_only, blurb);
         name_obj = new Name(this, name_obj, name);
         if (!entry) print("Your blurb has been %s to [%s].\n", over ?
                           "truncated" : "set", blurb);
         return over;
      } else {
         if (entry || blurb[0]) {
            blurb[0] = 0;
            strcpy(name, name_only);
            name_obj = new Name(this, name_obj, name);
            if (!entry) output("Your blurb has been turned off.\n");
         } else {
            if (!entry) output("Your blurb was already turned off.\n");
         }
      }
   } else if (entry) {
      blurb[0] = 0;
      strcpy(name, name_only);
      name_obj = new Name(this, name_obj, name);
   } else {
      if (blurb[0]) {
         if (!entry) print("Your blurb is currently set to [%s].\n", blurb);
      } else {
         if (!entry) output("You do not currently have a blurb set.\n");
      }
   }
   return 0;
}

void Session::DoHelp()			// Do /help command.
{
   output("Currently known commands:\n\n"
          "/blurb -- set a descriptive blurb\n"
          "/bye -- leave Phoenix\n"
          "/date -- display current date and time\n"
          "/help -- gives this thrilling message\n"
          "/send -- specify default sendlist\n"
          "/signal -- turns public/private signals on/off\n"
          "/who -- gives a list of who is connected\n"
          "No other /commands are implemented yet. [except /why! :-)]\n\n"
          "There are two ways to specify a user to send a private message.  "
          "You can use\n"
          "either a '#' and the fd number for the user, (as listed by /who) "
          "or any\n"
          "substring of the user's name. (case-insensitive)  Follow either "
          "form with\n"
          "a semicolon or colon and the message. (e.g. \"#4;hi\", \"dev;hi\","
          " ...)\n\n"
          "Any other line not beginning with a slash is simply sent to "
          "everyone.\n\n"
          "The following are recognized as smileys instead of as sendlists:"
          "\n\n\t:-) :-( :-P ;-) :_) :_( :) :( :P ;)\n\n");
}

void Session::DoReset()			// Do <space><return> idle time reset.
{
   ResetIdle(1);
}

void Session::DoMessage(const char *line) // Do message send.
{
   bool is_explicit;
   char sendlist[SendlistLen];

   const char *p = message_start(line, sendlist, SendlistLen, is_explicit);

   // Use last sendlist if none specified.
   if (!*sendlist) {
      if (*last_sendlist) {
         strcpy(sendlist, last_sendlist);
      } else {
         output("\a\aYou have no previous sendlist. (message not sent)\n");
         return;
      }
   }

   // Use default sendlist if indicated.
   if (!strcasecmp(sendlist, "default")) {
      if (*default_sendlist) {
         strcpy(sendlist, default_sendlist);
      } else {
         output("\a\aYou have no default sendlist. (message not sent)\n");
         return;
      }
   }

   // Save last sendlist if is_explicit.
   if (is_explicit && *sendlist) {
      strncpy(last_sendlist, sendlist, SendlistLen);
      last_sendlist[SendlistLen - 1] = 0;
   }

   if (!strcasecmp(sendlist, "everyone")) {
      SendEveryone(p);
   } else {
      SendPrivate(sendlist, p);
   }
}

// Send a message to everyone else signed on.
void Session::SendEveryone(const char *msg)
{
   int sent = 0;
   Session *session;
   for (session = sessions; session; session = session->next) {
      if (session == this) continue;
      session->Enqueue(new Message(PublicMessage, name_obj, msg));
      sent++;
   }

   switch (sent) {
   case 0:
      print("\a\aThere is no one else here! (message not sent)\n");
      break;
   case 1:
      ResetIdle(10);			// reset idle time
      print("(message sent to everyone.) [1 person]\n");
      break;
   default:
      ResetIdle(10);			// reset idle time
      print("(message sent to everyone.) [%d people]\n", sent);
      break;
   }
}

// Send private message by partial name match.
void Session::SendPrivate(const char *sendlist, const char *msg)
{
   if (!strcasecmp(sendlist, "me")) {
      ResetIdle(10);			// reset idle time
      print("(message sent to %s.)\n", name);
      Enqueue(new Message(PrivateMessage, name_obj, msg));
      return;
   }

   int matches = 0;
   Session *session, *dest, *extra;
   for (session = sessions; session; session = session->next) {
      if (strcasecmp(session->name_only, sendlist)) {
         if (match_name(session->name_only, sendlist)) {
            if (matches++) {
               extra = session;
            } else {
               dest = session;
            }
         }
      } else {				// Found exact match; use it.
         dest = session;
         matches = 1;
         break;
      }
   }

   switch (matches) {
   case 0:				// No matches.
      for (unsigned char *p = (unsigned char *) sendlist; *p; p++) {
         if (*p == UnquotedUnderscore) *p = Underscore;
      }
      print("\a\aNo names matched \"%s\". (message not sent)\n", sendlist);
      break;
   case 1:				// Found single match, send message.
      ResetIdle(10);			// reset idle time
      print("(message sent to %s.)\n", dest->name);
      dest->Enqueue(new Message(PrivateMessage, name_obj, msg));
      break;
   default:				// Multiple matches.
      print("\a\a\"%s\" matches %d names, including \"%s\" and \"%s\". "
            "(message not sent)\n", sendlist, matches, dest->name_only,
            extra->name_only);
      break;
   }
}

// Exit if shutting down and no users are left.
void Session::CheckShutdown()
{
   if (Shutdown && !sessions) {
      log_message("All connections closed, shutting down.");
      log_message("Server down.");
      if (logfile) fclose(logfile);
      exit(0);
   }
}
