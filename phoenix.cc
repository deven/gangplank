// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// phoenix.cc -- main server code.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Include files.
#include "block.h"
#include "fd.h"
#include "fdtable.h"
#include "listen.h"
#include "phoenix.h"
#include "session.h"
#include "telnet.h"
#include "user.h"

// Global variables.
int Shutdown;				// shutdown flag
FILE *logfile;				// log file

// XXX Should logfile use non-blocking code instead?

#ifdef NEED_STRERROR
const char *strerror(int err)
{
   static char msg[BufSize];

   if (err >= 0 && err < sys_nerr) {
      return sys_errlist[err];
   } else {
      sprintf(msg, "Error %d", err);
      return msg;
   }
}
#endif

// XXX class Date?
const char *date(time_t clock, int start, int len) // get part of date string
{
   static char buf[BufSize];

   if (!clock) time(&clock);		// get time if not passed
   strcpy(buf, ctime(&clock));		// make a copy of date string
   buf[24] = 0;				// ditch the newline
   if (len > 0 && len < 24) {
      buf[start + len] = 0;		// truncate further if requested
   }
   return buf + start;			// return (sub)string
}

// XXX class Log?
void OpenLog()				// open log file
{
   char buf[BufSize];
   time_t t;
   struct tm *tm;

   time(&t);
   if (!(tm = localtime(&t))) error("OpenLog(): localtime");
   sprintf(buf, "logs/%02d%02d%02d-%02d%02d%02d", tm->tm_year, tm->tm_mon + 1,
           tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
   if (!(logfile = fopen(buf, "a"))) error("OpenLog(): %s", buf);
   setlinebuf(logfile);
   unlink("log");
   symlink(buf, "log");
   fprintf(stderr, "Logging on \"%s\".\n", buf);
}

// XXX Use << operator instead of printf() formats?
void log_message(const char *format, ...) // log message
{
   char buf[BufSize];
   va_list ap;

   if (!logfile) return;
   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   (void) fprintf(logfile, "[%s] %s\n", date(0, 4, 15), buf);
}

void warn(const char *format, ...)	// print error message
{
   char buf[BufSize];
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   (void) fprintf(stderr, "\n%s: %s\n", buf, strerror(errno));
   (void) fprintf(logfile, "[%s] %s: %s\n", date(0, 4, 15), buf,
                  strerror(errno));
}

void error(const char *format, ...)	// print error message and exit
{
   char buf[BufSize];
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   (void) fprintf(stderr, "\n%s: %s\n", buf, strerror(errno));
   (void) fprintf(logfile, "[%s] %s: %s\n", date(0, 4, 15), buf,
                  strerror(errno));
   if (logfile) fclose(logfile);
   exit(1);
}

const char *message_start(const char *line, char *sendlist, int len,
                          int *is_explicit)
{
   const char *p;
   char state;
   int i;

   *is_explicit = 0;			// Assume implicit sendlist.

   // Attempt to detect smileys that shouldn't be sendlists...
   if (!isalpha(*line) && !isspace(*line)) {
      /* Only compare initial non-whitespace characters. */
      for (i = 0; i < len; i++) if (isspace(line[i])) break;

      // Just special-case a few smileys...
      if (!strncmp(line, ":-)", i) || !strncmp(line, ":-(", i) ||
          !strncmp(line, ":-P", i) || !strncmp(line, ";-)", i) ||
          !strncmp(line, ":_)", i) || !strncmp(line, ":_(", i) ||
          !strncmp(line, ":)",  i) || !strncmp(line, ":(",  i) ||
          !strncmp(line, ":P",  i) || !strncmp(line, ";)",  i) ||
          !strncmp(line, "(-:", i) || !strncmp(line, ")-:", i) ||
          !strncmp(line, "(-;", i) || !strncmp(line, "(_:", i) ||
          !strncmp(line, ")_:", i) || !strncmp(line, "(:",  i) ||
          !strncmp(line, "):",  i) || !strncmp(line, "(;",  i)) {
         strcpy(sendlist, "default");
         return line;
      }
   }

   // Doesn't appear to be a smiley, check for explicit sendlist.
   state = 0;
   i = 0;
   len--;
   for (p = line; *p; p++) {
      switch (state) {
      case 0:
         switch (*p) {
         case Space:
         case Tab:
            strcpy(sendlist, "default");
            return line + (*line == Space);
         case Colon:
         case Semicolon:
            sendlist[i] = 0;
            if (*++p == Space) p++;
            *is_explicit = 1;
            return p;
         case Backslash:
            state = Backslash;
            break;
         case Quote:
            state = Quote;
            break;
         case Underscore:
            if (i < len) sendlist[i++] = UnquotedUnderscore;
            break;
         default:
            if (i < len) sendlist[i++] = *p;
            break;
         }
         break;
      case Backslash:
         if (i < len) sendlist[i++] = *p;
         state = 0;
         break;
      case Quote:
         while (*p) {
            if (*p == Quote) {
               state = 0;
               break;
            } else {
               if (i < len) sendlist[i++] = *p++;
            }
         }
         break;
      }
   }
   strcpy(sendlist, "default");
   return line + (*line == Space);
}

int match_name(const char *name, const char *sendlist)
{
   const char *p, *q;

   if (!*name || !*sendlist) return 0;
   for (p = name, q = sendlist; *p && *q; p++, q++) {
      // Let an unquoted underscore match a space or an underscore.
      if (*q == char(UnquotedUnderscore) &&
          (*p == Space || *p == Underscore)) continue;
      if ((isupper(*p) ? tolower(*p) : *p) !=
          (isupper(*q) ? tolower(*q) : *q)) {
         // Mis-match, ignoring case. Recurse for middle matches.
         return match_name(name + 1, sendlist);
      }
   }
   return !*q;
}

void welcome(Telnet *telnet)
{
   // Make sure we're done with initial option negotiations.
   // Intentionally use == with bitfield mask to test both bits at once.
   if (telnet->LSGA == TelnetWillWont) return;
   if (telnet->RSGA == TelnetDoDont) return;
   if (telnet->echo == TelnetWillWont) return;

   // send welcome banner
   telnet->output("\nWelcome to Phoenix!\n\n");

   // Announce guest account.
   telnet->output("A \"guest\" account is available.\n\n");

   // Let's hope the SUPPRESS-GO-AHEAD option worked.
   if (!telnet->LSGA && !telnet->RSGA) {
      // Sigh.  Couldn't suppress Go Aheads.  Inform the user.
      telnet->output("Sorry, unable to suppress Go Aheads.  Must operate in "
                     "half-duplex mode.\n\n");
   }

   // Warn if about to shut down!
   if (Shutdown) {
      telnet->output("*** This server is about to shut down! ***\n\n");
   }

   // Send login prompt.
   telnet->Prompt("login: ");

   // Set user input processing function.
   telnet->SetInputFunction(login);
}

void login(Telnet *telnet, const char *line)
{
   // Check against hardcoded logins.
   // XXX stuff
   if (!strcasecmp(line, "/bye")) {
      // Exit Phoenix.
      if (telnet->Output.head) {
         // Queued output, try to send it first.
         telnet->blocked = 0;
         telnet->closing = 1;

         // Don't read any more from connection.
         telnet->NoReadSelect();

         // Do write to connection.
         telnet->WriteSelect();
      } else {
         // No queued output, close immediately.
         telnet->Close();
      }
      return;
   } else if (!strcasecmp(line, "guest")) {
      strcpy(telnet->session->user->user, line);
      strcpy(telnet->session->user->password, "guest");
      telnet->session->name[0] = 0;
      telnet->session->user->priv = 0;

      // Prompt for name.
      telnet->output('\n');
      telnet->Prompt("Enter name: ");

      // Set name input routine.
      telnet->SetInputFunction(name);

      return;
   } else {
      int found = 0;
      char buf[256], *user, *passwd, *name, *priv, *p;
      FILE *pw = fopen("passwd", "r");
      if (pw) {
         while (fgets(buf, 256, pw)) {
            if (buf[0] == '#') continue;
            p = user = buf;
            passwd = name = priv = 0;
            while (*p) if (*p == ':') { *p++ = 0; passwd = p; break; } else p++;
            while (*p) if (*p == ':') { *p++ = 0; name = p; break; } else p++;
            while (*p) if (*p == ':') { *p++ = 0; priv = p; break; } else p++;
            if (!priv) continue;
            if (!strcasecmp(line, user)) {
               found = 1;
               strcpy(telnet->session->user->user, user);
               strcpy(telnet->session->user->password, passwd);
               strcpy(telnet->session->name_only, name);
               telnet->session->user->priv = atoi(priv ? priv : "0");
               break;
            }
         }
      }
      fclose(pw);
      if (!found) {
         telnet->output("Login incorrect.\n");
         telnet->Prompt("login: ");
         return;
      }
   }

   // Disable echoing.
   telnet->do_echo = false;

   // Warn if echo wasn't turned off.
   if (!telnet->echo) telnet->print("\n\aSorry, password WILL echo.\n\n");

   // Prompt for password.
   telnet->Prompt("Password: ");

   // Set password input routine.
   telnet->SetInputFunction(password);
}

void password(Telnet *telnet, const char *line)
{
   // Send newline.
   telnet->output(Newline);

   // Check against encrypted password.
   char *password = telnet->session->user->password;
   if (strcmp(crypt(line, password), password)) {
      // Login incorrect.
      telnet->output("Login incorrect.\n");
      telnet->Prompt("login: ");

      // Enable echoing.
      telnet->do_echo = true;

      // Set login input routine.
      telnet->SetInputFunction(login);
   } else {
      // XXX stuff
      telnet->print("\nYour default name is \"%s\".\n",
                    telnet->session->name_only);

      // Enable echoing.
      telnet->do_echo = true;

      // Prompt for name.
      telnet->output(Newline);
      telnet->Prompt("Enter name: ");

      // Set name input routine.
      telnet->SetInputFunction(name);
   }
}

void name(Telnet *telnet, const char *line)
{
   if (!*line) {
      // blank line
      if (!strcasecmp(telnet->session->user->user, "guest")) {
         // Prompt for name.
         telnet->output(Newline);
         telnet->Prompt("Enter name: ");
         return;
      }
   } else {
      // Save user's name.
      strncpy(telnet->session->name_only, line, NameLen);
      telnet->session->name_only[NameLen - 1] = 0;
   }

   // Prompt for blurb.
   telnet->Prompt("Enter blurb: ");

   // Set name input routine.
   telnet->SetInputFunction(blurb);
}

void blurb(Telnet *telnet, const char *line)
{
   int over;

   if (!*line) line = telnet->session->user->default_blurb;
   if (*line) {
      over = strlen(telnet->session->name_only) + strlen(line) + 4 - NameLen;
      if (over > 0) {
         telnet->print("The combination of your name and blurb is %d character"
                       "%s too long.\n", over, over == 1 ? "" : "s");
         // Prompt for blurb.
         telnet->Prompt("Enter blurb: ");
         return;
      } else {
         // Save user's name, with blurb.
         strcpy(telnet->session->blurb, line);
         sprintf(telnet->session->name, "%s [%s]", telnet->session->name_only,
                 telnet->session->blurb);
      }
   } else {
      // Save user's name, no blurb.
      telnet->session->blurb[0] = 0;
      strcpy(telnet->session->name, telnet->session->name_only);
   }

   // Announce entry.
   Session::notify("*** %s has entered Phoenix! [%s] ***\n",
                   telnet->session->name,
                   date(time(&telnet->session->login_time), 11, 5));
   telnet->session->idle_since = telnet->session->login_time;
   log_message("Enter: %s (%s) on fd #%d.", telnet->session->name_only,
               telnet->session->user->user, telnet->fd);

   telnet->session->Link();		// Link new session into list.

   // Print welcome banner and do a /who list.
   telnet->output("\n\nWelcome to Phoenix.  Type \"/help\" for a list of "
                  "commands.\n\n");
   Session::who_cmd(telnet);

   // Set normal input routine.
   telnet->SetInputFunction(process_input);
}

void process_input(Telnet *telnet, const char *line)
{
   // XXX Make ! normal for average users?  normal if not a valid command?
   if (*line == '!') {
      // XXX add !priv command? do individual privilege levels?
      if (telnet->session->user->priv < 50) {
         telnet->output("Sorry, all !commands are privileged.\n");
         return;
      }
      if (!strncasecmp(line, "!down", 5)) {
         if (!strcasecmp(line, "!down !")) {
            log_message("Immediate shutdown requested by %s (%s).",
                        telnet->session->name_only,
                        telnet->session->user->user);
            log_message("Final shutdown warning.");
            Telnet::announce("*** %s has shut down Phoenix! ***\n",
                             telnet->session->name);
            Telnet::announce("\a\a>>> Server shutting down NOW!  Goodbye. <<<"
                             "\n\a\a");
            alarm(5);
            Shutdown = 2;
         } else if (!strcasecmp(line, "!down cancel")) {
            if (Shutdown) {
               Shutdown = 0;
               alarm(0);
               log_message("Shutdown cancelled by %s (%s).",
                           telnet->session->name_only,
                           telnet->session->user->user);
               Telnet::announce("*** %s has cancelled the server shutdown. ***"
                                "\n", telnet->session->name);
            } else {
               telnet->output("The server was not about to shut down.\n");
            }
         } else {
            int i;

            if (sscanf(line + 5, "%d", &i) != 1) i = 30;
            log_message("Shutdown requested by %s (%s) in %d seconds.",
                        telnet->session->name_only,
                        telnet->session->user->user, i);
            Telnet::announce("*** %s has shut down Phoenix! ***\n",
                             telnet->session->name);
            Telnet::announce("\a\a>>> This server will shutdown in %d seconds"
                             "... <<<\n\a\a", i);
            alarm(i);
            Shutdown = 1;
         }
      } else if (!strncasecmp(line, "!nuke ", 6)) {
         int i;

         if (sscanf(line + 6, "%d", &i) == 1) {
            Telnet::nuke(telnet, i < 0 ? -i : i, i >= 0);
         } else {
            telnet->print("Bad fd #: \"%s\"\n", line + 6);
         }
      } else {
         // Unknown !command.
         telnet->output("Unknown !command.\n");
      }
   } else if (*line == '/') {
      if (!strncasecmp(line, "/bye", 4)) {
         // Exit Phoenix.
         if (telnet->Output.head) {
            // Queued output, try to send it first.
            telnet->blocked = false;
            telnet->closing = true;

            // Don't read any more from connection.
            telnet->NoReadSelect();

            // Do write to connection.
            telnet->WriteSelect();
         } else {
            // No queued output, close immediately.
            telnet->Close();
         }
      } else if (!strncasecmp(line, "/who", 4)) {
         // /who list.
         Session::who_cmd(telnet);
      } else if (!strcasecmp(line, "/date")) {
         // Print current date and time.
         telnet->print("%s\n", date(0, 0, 0));
      } else if (!strncasecmp(line, "/signal", 7)) {
         const char *p = line + 7;
         while (*p && isspace(*p)) p++;
         if (!strncasecmp(p, "on", 2)) {
            telnet->SignalPublic = true;
            telnet->SignalPrivate = true;
            telnet->output("All signals are now on.\n");
         } else if (!strncasecmp(p, "off", 3)) {
            telnet->SignalPublic = false;
            telnet->SignalPrivate = false;
            telnet->output("All signals are now off.\n");
         } else if (!strncasecmp(p, "public", 6)) {
            p += 6;
            while (*p && isspace(*p)) p++;
            if (!strncasecmp(p, "on", 2)) {
               telnet->SignalPublic = true;
               telnet->output("Signals for public messages are now on.\n");
            } else if (!strncasecmp(p, "off", 3)) {
               telnet->SignalPublic = false;
               telnet->output("Signals for public messages are now off.\n");
            } else {
               telnet->output("/signal public syntax error!\n");
            }
         } else if (!strncasecmp(p, "private", 7)) {
            p += 7;
            while (*p && isspace(*p)) p++;
            if (!strncasecmp(p, "on", 2)) {
               telnet->SignalPrivate = true;
               telnet->output("Signals for private messages are now on.\n");
            } else if (!strncasecmp(p, "off", 3)) {
               telnet->SignalPrivate = false;
               telnet->output("Signals for private messages are now off.\n");
            } else {
               telnet->output("/signal private syntax error!\n");
            }
         } else {
            telnet->output("/signal syntax error!\n");
         }
      } else if (!strncasecmp(line, "/send", 5)) {
         const char *p = line + 5;
         while (*p && isspace(*p)) p++;
         if (!*p) {
            // Display current sendlist.
            if (!telnet->session->default_sendlist[0]) {
               telnet->print("Your default sendlist is turned off.\n");
            } else if (!strcasecmp(telnet->session->default_sendlist,
                                   "everyone")) {
               telnet->print("You are sending to everyone.\n");
            } else {
               telnet->print("Your default sendlist is set to \"%s\".\n",
                     telnet->session->default_sendlist);
            }
         } else if (!strcasecmp(p, "off")) {
            telnet->session->default_sendlist[0] = 0;
            telnet->print("Your default sendlist has been turned off.\n");
         } else if (!strcasecmp(p, "everyone")) {
            strcpy(telnet->session->default_sendlist, p);
            telnet->print("You are now sending to everyone.\n");
         } else {
            strncpy(telnet->session->default_sendlist, p, SendlistLen);
            telnet->session->default_sendlist[SendlistLen - 1] = 0;
            telnet->print("Your default sendlist is now set to \"%s\".\n",
                  telnet->session->default_sendlist);
         }
      } else if (!strncasecmp(line, "/why", 4)) {
         telnet->output("Why not?\n");
      } else if (!strncasecmp(line, "/blurb", 3)) {
         const char *start = line, *end;
         int len = NameLen - strlen(telnet->session->name_only) - 4;

         while (*start && !isspace(*start)) start++;
         while (*start && isspace(*start)) start++;
         if (*start) {
            for (const char *p = start; *p; p++) if (!isspace(*p)) end = p;
            if (strncasecmp(start, "off", end - start + 1)) {
               if ((*start == '\"' && *end == '\"' && start < end) ||
                   (*start == '[' && *end == ']')) start++; else end++;
               if (end - start < len) len = end - start;
               strncpy(telnet->session->blurb, start, len);
               telnet->session->blurb[len] = 0;
               sprintf(telnet->session->name, "%s [%s]",
                       telnet->session->name_only, telnet->session->blurb);
               telnet->print("Your blurb has been %s to [%s].\n",
                             end - start > len ? "truncated" : "set",
                             telnet->session->blurb);
            } else {
               if (telnet->session->blurb[0]) {
                  telnet->session->blurb[0] = 0;
                  strcpy(telnet->session->name, telnet->session->name_only);
                  telnet->output("Your blurb has been turned off.\n");
               } else {
                  telnet->output("Your blurb was already turned off.\n");
               }
            }
         } else {
            if (telnet->session->blurb[0]) {
               telnet->print("Your blurb is currently set to [%s].\n",
                              telnet->session->blurb);
            } else {
               telnet->output("You do not currently have a blurb set.\n");
            }
         }
      } else if (!strncasecmp(line, "/help", 5)) {
         telnet->output("Currently known commands:\n\n"
                        "/blurb -- set a descriptive blurb\n"
                        "/bye -- leave Phoenix\n"
                        "/date -- display current date and time\n"
                        "/help -- gives this thrilling message\n"
                        "/send -- specify default sendlist\n"
                        "/signal -- turns public/private signals on/off\n"
                        "/who -- gives a list of who is connected\n"
                        "No other /commands are implemented yet. "
                        "(except /why)\n\n"
                        "There are two ways to specify a user to send a "
                        "private message.  You can use\n"
                        "either a '#' and the fd number for the user, (as "
                        "listed by /who) or an\n"
                        "substring of the user's name. (case-insensitive)  "
                        "Follow either form with\n"
                        "a semicolon or colon and the message. (e.g. "
                        "\"#4;hi\", \"dev;hi\", ...)\n\n"
                        "Any other line not beginning with a slash is "
                        "simply sent to everyone.\n\n"
                        "The following are recognized as smileys instead of "
                        "as sendlists:\n\n"
                        "\t:-) :-( :-P ;-) :_) :_( :) :( :P ;) (-: )-: (-; "
                        "(_: )_: (: ): (;\n\n");
      } else {
         // Unknown /command.
         telnet->output("Unknown /command.  Type /help for help.\n");
      }
   } else if (!strcmp(line, " ")) {
      if (telnet->session->ResetIdle(1)) {
         telnet->print("Your idle time has been reset.\n");
      }
   } else if (*line) {
      int is_explicit;
      int i;
      char c;
      const char *p;
      char sendlist[SendlistLen];

      // Find the start of the message.
      p = message_start(line, sendlist, SendlistLen, &is_explicit);

      // Use last sendlist if none specified.
      if (!*sendlist) {
         if (*telnet->session->last_sendlist) {
            strcpy(sendlist, telnet->session->last_sendlist);
         } else {
            telnet->print("\a\aYou have no previous sendlist. (message not "
                          "sent)\n");
            return;
         }
      }

      if (!strcasecmp(sendlist, "default")) {
         if (*telnet->session->default_sendlist) {
            strcpy(sendlist, telnet->session->default_sendlist);
         } else {
            telnet->print("\a\aYou have no default sendlist. (message not "
                          "sent)\n");
            return;
         }
      }

      if (sscanf(sendlist, "#%d%c", &i, &c) == 1) {
         telnet->session->SendByFD(i, sendlist, is_explicit, p);
      } else if (!strcasecmp(sendlist, "everyone")) {
         telnet->session->SendEveryone(p);
      } else {
         telnet->session->SendPrivate(sendlist, is_explicit, p);
      }
   }
}

void quit(int sig)			// received SIGQUIT or SIGTERM
{
   log_message("Shutdown requested by signal in 30 seconds.");
   Telnet::announce("\a\a>>> This server will shutdown in 30 seconds... <<<"
                    "\n\a\a");
   alarm(30);
   Shutdown = 1;
}

void alrm(int sig)			// received SIGALRM
{
   // Ignore unless shutting down.
   if (Shutdown) {
      if (Shutdown == 1) {
         log_message("Final shutdown warning.");
         Telnet::announce("\a\a>>> Server shutting down NOW!  Goodbye. <<<"
                          "\n\a\a");
         alarm(5);
         Shutdown++;
      } else {
         log_message("Server down.");
         if (logfile) fclose(logfile);
         exit(0);
      }
   }
}

int main(int argc, char **argv)		// main program
{
   int pid;				// server process number

   Shutdown = 0;
   if (chdir(HOME)) error(HOME);
   OpenLog();
   Listen::Open(Port);

   // fork subprocess and exit parent
   if (argc < 2 || strcmp(argv[1], "-debug")) {
      switch (pid = fork()) {
      case 0:
         setpgrp();
#ifdef USE_SIGIGNORE
         sigignore(SIGHUP);
         sigignore(SIGINT);
         sigignore(SIGPIPE);
#else
         signal(SIGHUP, SIG_IGN);
         signal(SIGINT, SIG_IGN);
         signal(SIGPIPE, SIG_IGN);
#endif
         signal(SIGQUIT, quit);
         signal(SIGTERM, quit);
         signal(SIGALRM, alrm);
         log_message("Server started, running on port %d. (pid %d)", Port,
                     getpid());
         break;
      case -1:
         error("main(): fork()");
         break;
      default:
         fprintf(stderr, "Server started, running on port %d. (pid %d)\n",
                 Port, pid);
         exit(0);
         break;
      }
   }

   while(1) {
      Session::CheckShutdown();
      FD::Select();
   }
}
