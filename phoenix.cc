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
                          bool &is_explicit)
{
   const char *p;
   char state;
   int i;

   is_explicit = false;			// Assume implicit sendlist.

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
            is_explicit = true;
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
