// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// phoenix.cc -- main server code.
//
// Copyright (c) 1992-1994 Deven T. Corzine
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
   if (errno >= 0 && errno < sys_nerr) {
      (void) fprintf(stderr, "\n%s: %s\n", buf, sys_errlist[errno]);
      (void) fprintf(logfile, "[%s] %s: %s\n", date(0, 4, 15), buf,
                     sys_errlist[errno]);
   } else {
      (void) fprintf(stderr, "\n%s: Error %d\n", buf, errno);
      (void) fprintf(logfile, "[%s] %s: Error %d\n", date(0, 4, 15), buf,
                     errno);
   }
}

void error(const char *format, ...)	// print error message and exit
{
   char buf[BufSize];
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   if (errno >= 0 && errno < sys_nerr) {
      (void) fprintf(stderr, "\n%s: %s\n", buf, sys_errlist[errno]);
      (void) fprintf(logfile, "[%s] %s: %s\n", date(0, 4, 15), buf,
                     sys_errlist[errno]);
   } else {
      (void) fprintf(stderr, "\n%s: Error %d\n", buf, errno);
      (void) fprintf(logfile, "[%s] %s: Error %d\n", date(0, 4, 15), buf,
                     errno);
   }
   if (logfile) fclose(logfile);
   exit(1);
}

void crash(const char *format, ...)	// XXX print error message and crash
{
   char buf[BufSize];
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   (void) fprintf(stderr, "\n%s\n", buf);
   (void) fprintf(logfile, "[%s] %s\n", date(0, 4, 15), buf);
   if (logfile) fclose(logfile);
   abort();
   exit(-1);
}

const char *message_start(const char *line, char *sendlist, int len,
                          bool &is_explicit)
{
   const char *p;
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
          !strncmp(line, ":P",  i) || !strncmp(line, ";)",  i)) {
         strcpy(sendlist, "default");
         return line;
      }
   }

   // Doesn't appear to be a smiley, check for explicit sendlist.
   i = 0;
   len--;
   for (p = line; *p; p++) {
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
         if (*++p && i < len) sendlist[i++] = *p;
         break;
      case Quote:
         while (*p) {
            if (*p == Quote) {
               break;
            } else if (*p == Backslash) {
               if (*++p && i < len) sendlist[i++] = *p;
            } else {
               if (i < len) sendlist[i++] = *p;
            }
            p++;
         }
         break;
      case Underscore:
         if (i < len) sendlist[i++] = UnquotedUnderscore;
         break;
      default:
         if (i < len) sendlist[i++] = *p;
         break;
      }
   }
   strcpy(sendlist, "default");
   return line + (*line == Space);
}

// Returns position of match or 0.
int match_name(const char *name, const char *sendlist)
{
   const char *start, *p, *q;

   if (!name || !sendlist || !*name || !*sendlist) return 0;
   for (start = name; *start; start++) {
      for (p = start, q = sendlist; *p && *q; p++, q++) {
         // Let an unquoted underscore match a space or an underscore.
         if (*q == char(UnquotedUnderscore) &&
             (*p == Space || *p == Underscore)) continue;
         if ((isupper(*p) ? tolower(*p) : *p) !=
             (isupper(*q) ? tolower(*q) : *q)) break;
      }
      if (!*q) return (start - name) + 1;
   }
   return 0;
}

void quit(int sig)			// received SIGQUIT or SIGTERM
{
   log_message("Shutdown requested by signal in 30 seconds.");
   Session::announce("\a\a>>> This server will shutdown in 30 seconds... <<<"
                    "\n\a\a");
   alarm(30);
   Shutdown = 1;
}

void alrm(int sig)			// received SIGALRM
{
   // Ignore unless shutting down.
   switch (Shutdown) {
   case 1:
      log_message("Final shutdown warning.");
      Session::announce("\a\a>>> Server shutting down NOW!  Goodbye. <<<\n"
                        "\a\a");
      alarm(5);
      Shutdown++;
      break;
   case 2:
      ShutdownServer();
   case 3:
      log_message("Final restart warning.");
      Session::announce("\a\a>>> Server restarting NOW!  Goodbye. <<<\n\a\a");
      alarm(5);
      Shutdown++;
      break;
    case 4:
      RestartServer();
   }
}

void RestartServer()			// Restart server.
{
   log_message("Restarting server.");
   if (logfile) fclose(logfile);
   FD::CloseAll();
   execl("conf", "conf", NULL);
   error("conf");
}

void ShutdownServer()			// Shutdown server.
{
   log_message("Server down.");
   if (logfile) fclose(logfile);
   exit(0);
}

int main(int argc, char **argv)		// main program
{
   int pid;				// server process number
   int port;				// TCP port to use

   Shutdown = 0;
   if (chdir(HOME)) error(HOME);
   OpenLog();
   port = argc > 1 ? atoi(argv[1]) : 0;
   if (!port) port = DefaultPort;
   Listen::Open(port);

   // fork subprocess and exit parent
   if (argc < 2 || strcmp(argv[1], "-debug")) {
      switch (pid = fork()) {
      case 0:
         setsid();
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
         log_message("Server started, running on port %d. (pid %d)", port,
                     getpid());
         break;
      case -1:
         error("main(): fork()");
         break;
      default:
         fprintf(stderr, "Server started, running on port %d. (pid %d)\n",
                 port, pid);
         exit(0);
         break;
      }
   } else {
      log_message("Server started, running on port %d. (pid %d)", port,
                  getpid());
   }

   while(1) {
      Session::CheckShutdown();
      FD::Select();
   }
}
