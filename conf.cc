// -*- C++ -*-
//
// Conferencing system server.
//
// conf.cc -- main server code.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Include files.
#include "conf.h"

// Global variables.
Session *sessions;			// active sessions
int Shutdown;				// shutdown flag
FDTable fdtable;			// File descriptor table.
fd_set readfds;				// read fdset for select()
fd_set writefds;			// write fdset for select()
FILE *logfile;				// log file

// XXX Should logfile use non-blocking code instead?

// Static variables.
static char buf[BufSize];		// temporary buffer
static char inbuf[BufSize];		// input buffer

#ifdef NEED_STRERROR
const char *strerror(int err)
{
   static char msg[32];

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
   static char buf[32];

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
   va_list ap;

   if (!logfile) return;
   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   (void) fprintf(logfile, "[%s] %s\n", date(0, 4, 15), buf);
}

void warn(const char *format, ...)	// print error message
{
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

void Telnet::Drain()			// Drain connection, then close.
{
   blocked = false;
   closing = true;
   NoReadSelect();
   WriteSelect();
}

void Telnet::LogCaller()		// Log calling host and port.
{
   struct sockaddr_in saddr;
   socklen_t saddrlen = sizeof(saddr);

   if (!getpeername(fd, (struct sockaddr *) &saddr, &saddrlen)) {
      log_message("Accepted connection on fd #%d from %s port %d.", fd,
                  inet_ntoa(saddr.sin_addr), saddr.sin_port);
   } else {
      warn("Telnet::LogCaller(): getpeername()");
   }
}

void Telnet::SaveInputLine(const char *line) // Save input line for later.
{
   Line *p;

   p = new Line(line);
   if (lines) {
      lines->Append(p);
   } else {
      lines = p;
   }
}

void Telnet::SetInputFunction(InputFuncPtr input) // Set user input function.
{
   Line *p;

   InputFunc = input;

   // Process lines as long as we still have a defined input function.
   while (InputFunc && lines) {
      p = lines;
      lines = p->next;
      InputFunc(this, p->line);
      delete p;
   }
}

void Telnet::output(int byte)		// queue output byte
{
   switch (byte) {
   case TelnetIAC:			// command escape: double it
      if (Output.out(TelnetIAC, TelnetIAC) && !blocked) WriteSelect();
      break;
   case Return:				// carriage return: send "\r\0"
      if (Output.out(Return, Null) && !blocked) WriteSelect();
      break;
   case Newline:			// newline: send "\r\n"
      if (Output.out(Return, Newline) && !blocked) WriteSelect();
      break;
   default:				// normal character: send it
      if (Output.out(byte) && !blocked) WriteSelect();
      break;
   }
}

void Telnet::output(const char *buf)	// queue output data
{
   int byte;

   if (!buf || !*buf) return;		// return if no data
   output(*((unsigned const char *) buf++)); // Handle WriteSelect().
   while (*buf) {
      switch (byte = *((unsigned const char *) buf++)) {
      case TelnetIAC:			// command escape: double it
         Output.out(TelnetIAC, TelnetIAC);
         break;
      case Return:			// carriage return: send "\r\0"
         Output.out(Return, Null);
         break;
      case Newline:			// newline: send "\r\n"
         Output.out(Return, Newline);
         break;
      default:				// normal character: send it
         Output.out(byte);
         break;
      }
   }
}

void Telnet::output(const char *buf, int len) // queue output data (with length)
{
   int byte;

   if (!buf || !len) return;		// return if no data
   output(*((unsigned const char *) buf++)); // Handle WriteSelect().
   while (--len) {
      switch (byte = *((unsigned const char *) buf++)) {
      case TelnetIAC:			// command escape: double it
         Output.out(TelnetIAC, TelnetIAC);
         break;
      case Return:			// carriage return: send "\r\0"
         Output.out(Return, Null);
         break;
      case Newline:			// newline: send "\r\n"
         Output.out(Return, Newline);
         break;
      default:				// normal character: send it
         Output.out(byte);
         break;
      }
   }
}

void Telnet::print(const char *format, ...) // formatted write
{
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   output(buf);
}

void Telnet::command(int byte)		// Queue command byte.
{
   WriteSelect();			// Always write for command output.
   Command.out(byte);			// Queue command byte.
}

void Telnet::command(int byte1, int byte2) // Queue 2 command bytes.
{
   WriteSelect();			// Always write for command output.
   Command.out(byte1, byte2);		// Queue 2 command bytes.
}

void Telnet::command(int byte1, int byte2, int byte3) // Queue 3 command bytes.
{
   WriteSelect();			// Always write for command output.
   Command.out(byte1, byte2, byte3);	// Queue 3 command bytes.
}

void Telnet::command(const char *buf)	// queue command data
{
   if (!buf || !*buf) return;		// return if no data
   WriteSelect();			// Always write for command output.
   while (*buf) Command.out(*((unsigned const char *) buf++));
}

void Telnet::command(const char *buf, int len) // queue command data (w/length)
{
   if (!buf || !*buf) return;		// return if no data
   WriteSelect();			// Always write for command output.
   while (len--) Command.out(*((unsigned const char *) buf++));
}

void Telnet::OutputWithRedraw(const char *buf) // queue output w/redraw
{
   UndrawInput();
   output(buf);
   RedrawInput();
}

void Telnet::PrintWithRedraw(const char *format, ...) // format output w/redraw
{
   va_list ap;

   UndrawInput();
   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   output(buf);
   RedrawInput();
}

FDTable::FDTable()			// constructor
{
   used = 0;
   size = getdtablesize();
   array = new FD *[size];
   for (int i = 0; i < size; i++) array[i] = NULL;
}

FDTable::~FDTable()			// destructor
{
   for (int i = 0; i < used; i++) {
      if (array[i]) delete array[i];
   }
   delete array;
}

void FDTable::OpenListen(int port)	// Open a listening port.
{
   Listen *l = new Listen(port);
   if (l->fd < 0 || l->fd >= size) {
      error("FDTable::OpenListen(port = %d): fd #%d: range error! [0-%d]",
            port, l->fd, size - 1);
   }
   if (l->fd >= used) used = l->fd + 1;
   array[l->fd] = l;
   l->ReadSelect();
}

void FDTable::OpenTelnet(int lfd)	// Open a telnet connection.
{
   Telnet *t = new Telnet(lfd);
   if (t->fd < 0 || t->fd >= size) {
      warn("FDTable::OpenTelnet(lfd = %d): fd #%d: range error! [0-%d]", lfd,
           t->fd, size - 1);
      delete t;
      return;
   }
   if (t->fd >= used) used = t->fd + 1;
   array[t->fd] = t;
}

void FDTable::Close(int fd)		// Close fd.
{
   if (fd < 0 || fd >= used) {
      error("FDTable::Close(fd = %d): range error! [0-%d]", fd, used - 1);
   }
   delete array[fd];
   array[fd] = NULL;
   if (fd == used - 1) {		// Fix highest used index if necessary.
      while (used > 0) {
         if (array[--used]) {
            used++;
            break;
         }
      }
   }
}

void FDTable::Select()			// Select across all ready connections.
{
   fd_set rfds = readfds;		// copy of readfds to pass to select()
   fd_set wfds = writefds;		// copy of writefds to pass to select()
   int found;				// number of file descriptors found

   found = select(size, &rfds, &wfds, NULL, NULL);

   if (found == -1) {
      if (errno == EINTR) return;
      error("FDTable::Select(): select()");
   }

   // Check for I/O ready on connections.
   for (int fd = 0; found && fd < used; fd++) {
      if (FD_ISSET(fd, &rfds)) {
         InputReady(fd);
         found--;
      }
      if (FD_ISSET(fd, &wfds)) {
         OutputReady(fd);
         found--;
      }
   }
}

void FDTable::InputReady(int fd)	// Input ready on file descriptor fd.
{
   if (fd < 0 || fd >= used) {
      error("FDTable::InputReady(fd = %d): range error! [0-%d]", fd, used - 1);
   }
   array[fd]->InputReady(fd);
}

void FDTable::OutputReady(int fd)	// Output ready on file descriptor fd.
{
   if (fd < 0 || fd >= used) {
      error("FDTable::OutputReady(fd = %d): range error! [0-%d]", fd,
            used - 1);
   }
   array[fd]->OutputReady(fd);
}

// Send announcement to everyone.  (Formatted write to all connections.)
void FDTable::announce(const char *format, ...)
{
   Telnet *t;
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   for (int i = 0; i < used; i++) {
      if ((t = (Telnet *) array[i]) && t->type == TelnetFD) {
         t->OutputWithRedraw(buf);
      }
   }
}

// Nuke a user (force close connection).
void FDTable::nuke(Telnet *telnet, int fd, int drain)
{
   Telnet *t;

   if (fd >= 0 && fd < used && (t = (Telnet *) array[fd]) &&
       t->type == TelnetFD) {
      t->UndrawInput();
      t->print("%c%c%c*** You have been nuked by %s. ***\n", Bell, Bell, Bell,
               telnet->session->name);
      log_message("%s (%s) on fd #%d has been nuked by %s (%s).",
                  t->session->name, t->session->user->user, fd,
                  telnet->session->name, telnet->session->user->user);
      t->nuke(telnet, drain);
   } else {
      telnet->print("There is no user on fd #%d.\n", fd);
   }
}

// Send private message by fd #.
void FDTable::SendByFD(Telnet *telnet, int fd, const char *sendlist,
                       int is_explicit, const char *msg)
{
   Telnet *t;

   // Save last sendlist if explicit.
   if (is_explicit && *sendlist) {
      strncpy(telnet->session->last_sendlist, sendlist, SendlistLen);
      telnet->session->last_sendlist[SendlistLen - 1] = 0;
   }

   if ((t = (Telnet *) array[fd]) && t->type == TelnetFD) {
      time(&telnet->session->idle_since); // reset idle tme
      telnet->print("(message sent to %s.)\n", t->session->name);
      t->PrintWithRedraw("%c\n >> Private message from %s: [%s]\n - %s\n",
                         Bell, telnet->session->name, date(0, 11, 5), msg);
   } else {
      telnet->print("%c%cThere is no user on fd #%d. (message not sent)\n",
                    Bell, Bell, fd);
   }
}

// Send public message to everyone.
void FDTable::SendEveryone(Telnet *telnet, const char *msg)
{
   Session *s;
   int sent;

   time(&telnet->session->idle_since); // reset idle time

   sent = 0;
   for (s = sessions; s; s = s->next) {
      if (s->telnet != telnet) {
         sent++;
         s->telnet->PrintWithRedraw("%c\n -> From %s to everyone: [%s]\n"
                                    " - %s\n", Bell, telnet->session->name,
                                    date(0, 11, 5), msg);
      }
   }

   switch (sent) {
   case 0:
      telnet->print("%c%cThere is no one else here! (message not sent)\n",
                    Bell, Bell);
      break;
   case 1:
      telnet->print("(message sent to everyone.) [1 person]\n");
      break;
   default:
      telnet->print("(message sent to everyone.) [%d people]\n", sent);
      break;
   }
}

// Send private message by partial name match.
void FDTable::SendPrivate(Telnet *telnet, const char *sendlist, int is_explicit,
                          const char *msg)
{
   Telnet *t, *dest;
   int matches, i;

   // Save last sendlist if explicit.
   if (is_explicit && *sendlist) {
      strncpy(telnet->session->last_sendlist, sendlist, SendlistLen);
      telnet->session->last_sendlist[SendlistLen - 1] = 0;
   }

   if (!strcmp(sendlist, "me")) {
      matches = 1;
      dest = telnet;
   } else {
      matches = 0;
      for (i = 0; i < used; i++) {
         if ((t = (Telnet *) array[i]) && t->type == TelnetFD &&
             match_name(t->session->name, sendlist)) {
            dest = t;
            matches++;
         }
      }
   }

   switch (matches) {
   case 0:				// No matches.
      for (unsigned char *p = (unsigned char *) sendlist; *p; p++) {
         if (*p == UnquotedUnderscore) *p = Underscore;
      }
      telnet->print("%c%cNo names matched \"%s\". (message not sent)\n",
                    Bell, Bell, sendlist);
      break;
   case 1:				// Found single match, send message.
      time(&telnet->session->idle_since); // reset idle tme
      telnet->print("(message sent to %s.)\n", dest->session->name);
      dest->PrintWithRedraw("%c\n >> Private message from %s: [%s]\n - %s\n",
                            Bell, telnet->session->name, date(0, 11, 5), msg);
      break;
   default:				// Multiple matches.
      telnet->print("\"%s\" matches %d names, including \"%s\". (message not "
                    "sent)\n", sendlist, matches, dest->session->name);
      break;
   }
}

void notify(const char *format, ...)	// formatted write to all sessions
{
   Session *session;
   va_list ap;

   va_start(ap, format);
   (void) vsprintf(buf, format, ap);
   va_end(ap);
   for (session = sessions; session; session = session->next) {
      session->telnet->OutputWithRedraw(buf);
   }
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
   for (p = line; *p && i < len; p++) {
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
            sendlist[i++] = UnquotedUnderscore;
            break;
         default:
            sendlist[i++] = *p;
            break;
         }
         break;
      case Backslash:
         sendlist[i++] = *p;
         state = 0;
         break;
      case Quote:
         while (*p && i < len) {
            if (*p == Quote) {
               state = 0;
               break;
            } else {
               sendlist[i++] = *p++;
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

// Set telnet ECHO option. (local)
void Telnet::set_echo(CallbackFuncPtr callback, int state)
{
   if (state) {
      command(TelnetIAC, TelnetWill, TelnetEcho);
      echo |= TelnetWillWont;		// mark WILL sent
   } else {
      command(TelnetIAC, TelnetWont, TelnetEcho);
      echo &= ~TelnetWillWont;		// mark WON'T sent
   }
   echo_callback = callback;		// save callback function
}

// Set telnet SUPPRESS-GO-AHEAD option. (local)
void Telnet::set_LSGA(CallbackFuncPtr callback, int state)
{
   if (state) {
      command(TelnetIAC, TelnetWill, TelnetSuppressGoAhead);
      LSGA |= TelnetWillWont;		// mark WILL sent
   } else {
      command(TelnetIAC, TelnetWont, TelnetSuppressGoAhead);
      LSGA &= ~TelnetWillWont;		// mark WON'T sent
   }
   LSGA_callback = callback;		// save callback function
}

// Set telnet SUPPRESS-GO-AHEAD option. (remote)
void Telnet::set_RSGA(CallbackFuncPtr callback, int state)
{
   if (state) {
      command(TelnetIAC, TelnetDo, TelnetSuppressGoAhead);
      RSGA |= TelnetDoDont;		// mark DO sent
   } else {
      command(TelnetIAC, TelnetDont, TelnetSuppressGoAhead);
      RSGA &= ~TelnetDoDont;		// mark DON'T sent
   }
   RSGA_callback = callback;		// save callback function
}

Listen::Listen(int port)		// Listen on a port.
{
   const int Backlog = 8;		// backlog on socket (for listen())
   struct sockaddr_in saddr;		// socket address
   int tries = 0;			// number of tries so far
   int option = 1;			// option to set for setsockopt()

   type = ListenFD;			// Identify as a Listen FD.

   // Initialize listening socket.
   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_family = AF_INET;
   saddr.sin_addr.s_addr = htonl(INADDR_ANY);
   saddr.sin_port = htons((u_short) port);
   if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
      error("Listen::Listen(): socket()");
   }
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option))) {
      error("Listen::Listen(): setsockopt()");
   }

   // Try to bind to the port.  Try real hard.
   while (1) {
      if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr))) {
         if (errno == EADDRINUSE) {
            switch (tries++) {
            case 0:
               // First time failed.  Try to shut down a running server.
               RequestShutdown(port);
               break;
            case 1:
               // From now on, just wait.  Announce it.
               fprintf(stderr, "Waiting for port %d.\n", port);
               break;
            default:
               // Still waiting.
               sleep(1);
               break;
            }
         } else {
            error("Listen::Listen(): bind(port = %d)", port);
         }
      } else {
         break;
      }
   }

   if (listen(fd, Backlog)) error("Listen::Listen(): listen()");
}

void Listen::RequestShutdown(int port)	// Connect to port, request shutdown.
{
   struct sockaddr_in saddr;		// socket address
   int fd;				// listening socket fd
   unsigned char c;			// character for simple I/O
   unsigned char state;			// state for processing input
   fd_set fds, fds2;			// fd_set for select() and copy
   struct timeval tv, tv2;		// timeval for select() timeout and copy

   // Connect to requested port.
   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_family = AF_INET;
   saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   saddr.sin_port = htons((u_short) port);
   if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
      error("Listen::RequestShutdown(): socket()");
   }
   if (connect(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
      close(fd);			// Connection failed, forget it.
      return;
   }

   // Connected, request shutdown from running server.
   fprintf(stderr, "Attempting to shut down running server.\n");

   // Send fake telnet command for shutdown.
   c = TelnetIAC;
   write(fd, &c, 1);
   c = ShutdownCommand;
   write(fd, &c, 1);

   // Wait for response.

   // Initialize fd_set.
   FD_ZERO(&fds2);
   FD_SET(fd, &fds2);

   // Initialize timeval structure for timeout. (10 seconds)
   tv2.tv_sec = 10;
   tv2.tv_usec = 0;

   // Start in default state.
   state = 0;

   // Try to get acknowledgement without waiting forever.
   for (fds = fds2, tv = tv2; select(fd + 1, &fds, NULL, NULL, &tv) == 1 &&
        read(fd, &c, 1) == 1; fds = fds2, tv = tv2) {
      switch (state) {
      case TelnetIAC:
         switch (c) {
         case ShutdownCommand:
            fprintf(stderr, "Shutdown request acknowledged.\n");
            close(fd);
            return;
         case TelnetWill:
         case TelnetWont:
         case TelnetDo:
         case TelnetDont:
            state = c;
            break;
         default:
            fprintf(stderr, "Shutdown request not acknowledged.\n");
            close(fd);
            return;
         }
         break;
      case TelnetWill:
      case TelnetWont:
      case TelnetDo:
      case TelnetDont:
         state = 0;
         break;
      default:
         if (c == TelnetIAC) {
            state = c;
         } else {
            fprintf(stderr, "Shutdown request not acknowledged.\n");
            close(fd);
            return;
         }
         break;
      }
   }
   fprintf(stderr, "Shutdown request not acknowledged.\n");
   close(fd);
   return;
}

void welcome(Telnet *telnet)
{
   // Make sure we're done with initial option negotiations.
   // Intentionally use == with bitfield mask to test both bits at once.
   if (telnet->LSGA == TelnetWillWont) return;
   if (telnet->RSGA == TelnetDoDont) return;
   if (telnet->echo == TelnetWillWont) return;

   // send welcome banner
   telnet->output("\nWelcome to conf!\n\n");

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
   if (!strcmp(line, "guest")) {
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
   } else if (!strcmp(line, "deven")) {
      // Password and all other user accounts have been redacted.
      strcpy(telnet->session->user->user, line);
      strcpy(telnet->session->user->password, "REDACTED");
      strcpy(telnet->session->name, "Deven");
      telnet->session->user->priv = 100;
   } else {
      telnet->output("Login incorrect.\n");
      telnet->Prompt("login: ");
      return;
   }

   // Disable echoing.
   telnet->do_echo = false;

   // Warn if echo wasn't turned off.
   if (!telnet->echo) telnet->print("\n%cSorry, password WILL echo.\n\n", Bell);

   // Prompt for password.
   telnet->Prompt("Password: ");

   // Set password input routine.
   telnet->SetInputFunction(password);
}

void password(Telnet *telnet, const char *line)
{
   // Send newline.
   telnet->output(Newline);

   // Check against hardcoded password.
   if (strcmp(line, telnet->session->user->password)) {
      // Login incorrect.
      telnet->output("Login incorrect.\n");
      telnet->Prompt("login: ");

      // Enable echoing.
      telnet->do_echo = true;

      // Set login input routine.
      telnet->SetInputFunction(login);
   } else {
      // XXX stuff
      telnet->print("\nYour default name is \"%s\".\n", telnet->session->name);

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
      if (!strcmp(telnet->session->user->user, "guest")) {
         // Prompt for name.
         telnet->output(Newline);
         telnet->Prompt("Enter name: ");
         return;
      }
   } else {
      // Save user's name.
      strncpy(telnet->session->name, line, NameLen);
      telnet->session->name[NameLen - 1] = 0;
   }

   // Link new session into list.
   telnet->session->next = sessions;
   sessions = telnet->session;

   // Announce entry.
   notify("*** %s has entered conf! [%s] ***\n", telnet->session->name,
          date(time(&telnet->session->login_time), 11, 5));
   telnet->session->idle_since = telnet->session->login_time;
   log_message("Enter: %s (%s) on fd #%d.", telnet->session->name,
               telnet->session->user->user, telnet->fd);

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
      if (!strncmp(line, "!down", 5)) {
         if (!strcmp(line, "!down !")) {
            log_message("Immediate shutdown requested by %s (%s).",
                telnet->session->name, telnet->session->user->user);
            log_message("Final shutdown warning.");
            fdtable.announce("*** %s has shut down conf! ***\n",
                             telnet->session->name);
            fdtable.announce("%c%c>>> Server shutting down NOW!  Goodbye. <<<\n"
                             "%c%c", Bell, Bell, Bell, Bell);
            alarm(5);
            Shutdown = 2;
         } else if (!strcmp(line, "!down cancel")) {
            if (Shutdown) {
               Shutdown = 0;
               alarm(0);
               log_message("Shutdown canceled by %s (%s).",
                           telnet->session->name, telnet->session->user->user);
               fdtable.announce("*** %s has cancelled the server shutdown. ***"
                                "\n", telnet->session->name);
            } else {
               telnet->output("The server was not about to shut down.\n");
            }
         } else {
            int i;

            if (sscanf(line + 5, "%d", &i) != 1) i = 30;
            log_message("Shutdown requested by %s (%s) in %d seconds.",
                        telnet->session->name, telnet->session->user->user, i);
            fdtable.announce("*** %s has shut down conf! ***\n",
                             telnet->session->name);
            fdtable.announce("%c%c>>> This server will shutdown in %d "
                             "seconds... <<<\n%c%c", Bell, Bell, i, Bell, Bell);
            alarm(i);
            Shutdown = 1;
         }
      } else if (!strncmp(line, "!nuke ", 6)) {
         int i;

         if (sscanf(line + 6, "%d", &i) == 1) {
            fdtable.nuke(telnet, i < 0 ? -i : i, i >= 0);
         } else {
            telnet->print("Bad fd #: \"%s\"\n", line + 6);
         }
      } else {
         // Unknown !command.
         telnet->output("Unknown !command.\n");
      }
   } else if (*line == '/') {
      if (!strncmp(line, "/bye", 4)) {
         // Exit conf.
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
      } else if (!strncmp(line, "/who", 4)) {
         // /who list.
         who_cmd(telnet);
      } else if (!strcmp(line, "/date")) {
         // Print current date and time.
         telnet->print("%s\n", date(0, 0, 0));
      } else if (!strncmp(line, "/send", 5)) {
         const char *p = line + 5;
         while (*p && isspace(*p)) p++;
         if (!*p) {
            // Display current sendlist.
            if (!telnet->session->default_sendlist[0]) {
               telnet->print("Your default sendlist is turned off.\n");
            } else if (!strcmp(telnet->session->default_sendlist, "everyone")) {
               telnet->print("You are sending to everyone.\n");
            } else {
               telnet->print("Your default sendlist is set to \"%s\".\n",
                     telnet->session->default_sendlist);
            }
         } else if (!strcmp(p, "off")) {
            telnet->session->default_sendlist[0] = 0;
            telnet->print("Your default sendlist has been turned off.\n");
         } else if (!strcmp(p, "everyone")) {
            strcpy(telnet->session->default_sendlist, p);
            telnet->print("You are now sending to everyone.\n");
         } else {
            strncpy(telnet->session->default_sendlist, p, 31);
            telnet->session->default_sendlist[31] = 0;
            telnet->print("Your default sendlist is now set to \"%s\".\n",
                  telnet->session->default_sendlist);
         }
      } else if (!strncmp(line, "/help", 5)) {
         // help?  ha!
         telnet->output("Help?  Help?!?  This program isn't done, you know.\n"
                        "\nOnly known commands:\n\n"
                        "/bye -- leave conf\n"
                        "/date -- display current date and time\n"
                        "/send -- specify default sendlist\n"
                        "/who -- gives trivial list of who is connected\n"
                        "/help -- gives this dumb message\n\n"
                        "No other /commands are implemented yet.\n\n"
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
      int idle;

      idle = (time(NULL) - telnet->session->idle_since) / 60;
      time(&telnet->session->idle_since); // reset idle time
      if (idle) telnet->print("Your idle time has been reset.\n");
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
            telnet->print("%c%cYou have no previous sendlist. (message not "
                          "sent)\n", Bell, Bell);
            return;
         }
      }

      if (!strcmp(sendlist, "default")) {
         if (*telnet->session->default_sendlist) {
            strcpy(sendlist, telnet->session->default_sendlist);
         } else {
            telnet->print("%c%cYou have no default sendlist. (message not "
                          "sent)\n", Bell, Bell);
            return;
         }
      }

      if (sscanf(sendlist, "#%d%c", &i, &c) == 1) {
         fdtable.SendByFD(telnet, i, sendlist, is_explicit, p);
      } else if (!strcmp(sendlist, "everyone")) {
         fdtable.SendEveryone(telnet, p);
      } else {
         fdtable.SendPrivate(telnet, sendlist, is_explicit, p);
      }
   }
}

void who_cmd(Telnet *telnet)
{
   Session *s;
   Telnet *t;
   int idle;

   // Output /who header.
   telnet->output("\n"
          " Name                              On Since  Idle  User      fd\n"
          " ----                              --------  ----  ----      --\n");

   // Output data about each user.
   for (s = sessions; s; s = s->next) {
      t = s->telnet;
      idle = (time(NULL) - t->session->idle_since) / 60;
      if (idle) {
         telnet->print(" %-32s  %8s  %4d  %-8s  %2d\n", t->session->name,
                       date(t->session->login_time, 11, 8), idle,
                       t->session->user->user, t->fd);
      } else {
         telnet->print(" %-32s  %8s        %-8s  %2d\n", t->session->name,
                       date(t->session->login_time, 11, 8),
                       t->session->user->user, t->fd);
      }
   }
}

Telnet::Telnet(int lfd)			// Telnet constructor.
{
   type = TelnetFD;			// Identify as a Telnet FD.
   session = NULL;			// no Session (yet)
   data = new char[InputSize];		// Allocate input line buffer.
   end = data + InputSize;		// Save end of allocated block.
   point = free = data;			// Mark input line as empty.
   mark = NULL;				// No mark set initially.
   prompt = NULL;			// No prompt initially.
   prompt_len = 0;			// Length of prompt
   lines = NULL;			// No pending input lines.
   InputFunc = NULL;			// No input function.
   state = 0;				// telnet input state = 0 (data)
   undrawn = false;			// Input line not undrawn.
   blocked = false;			// output not blocked
   closing = false;			// connection not closing
   do_echo = true;			// Do echoing, if ECHO option enabled.
   echo = 0;				// ECHO option off (local)
   LSGA = 0;				// local SUPPRESS-GO-AHEAD option off
   RSGA = 0;				// remote SUPPRESS-GO-AHEAD option off
   echo_callback = NULL;		// no ECHO callback (local)
   LSGA_callback = NULL;		// no local SUPPRESS-GO-AHEAD callback
   RSGA_callback = NULL;		// no remote SUPPRESS-GO-AHEAD callback

   fd = accept(lfd, NULL, NULL);	// Accept TCP connection.
   if (fd == -1) return;		// Return if failed.

   LogCaller();				// Log calling host and port.
   NonBlocking();			// Place fd in non-blocking mode.

   session = new Session(this);		// Create a new Session.

   ReadSelect();			// Select new connection for reading.

   set_LSGA(welcome, true);		// Start initial options negotiations.
   set_RSGA(welcome, true);
   set_echo(welcome, true);
}

void Telnet::Prompt(const char *p)	// Print and set new prompt.
{
   prompt_len = strlen(p);
   if (prompt) delete prompt;
   prompt = new char[prompt_len + 1];
   strcpy(prompt, p);
   if (!undrawn) output(prompt);
}

Session::Session(Telnet *t)
{
   time_t now;				// current time

   telnet = t;				// Save Telnet pointer.
   next = NULL;				// No next session yet.
   name[0] = 0;				// No name yet.

   strcpy(default_sendlist, "everyone"); // Default sendlist is "everyone".
   last_sendlist[0] = 0;		// No previous sendlist yet.
   login_time = time(&now);		// Not logged in yet.
   idle_since = now;			// Reset idle time.

   user = new User(this);		// Create a new User for this Session.
}

User::User(Session *s)
{
   priv = 10;				// default user privilege level
   strcpy(user, "[nobody]");		// Who is this?
   password[0] = 0;			// No password.
   reserved_name[0] = 0;		// No name.
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
      notify("*** %s has left conf! [%s] ***\n", name, date(0, 11, 5));
      log_message("Exit: %s (%s) on fd #%d.", name, user->user, telnet->fd);
   }

   delete user;
}

Telnet::~Telnet()
{
   delete session;			// Free session structure.
   delete data;				// Free input line buffer.

   if (fd != -1) close(fd);		// Close connection.

   NoReadSelect();			// Don't select closed connections!
   NoWriteSelect();
}

// Nuke a user (force close connection).
void Telnet::nuke(Telnet *telnet, int drain)
{
   telnet->print("User \"%s\" (%s) on fd #%d has been nuked.\n", session->name,
                 session->user->user, fd);
   if (Output.head && drain) {
      Drain();
   } else {
      Close();
   }
}

void Telnet::UndrawInput()		// Erase input line from screen.
{
   int lines;

   if (echo == TelnetEnabled && do_echo && !undrawn && End()) {
      undrawn = true;
      lines = PointLine();
      // XXX ANSI!
      if (lines) {
         print("\r\033[%dA\033[J", lines); // Move cursor up and erase.
      } else {
         output("\r\033[J");		// Erase line.
      }
   }
}

void Telnet::RedrawInput()		// Redraw input line on screen.
{
   int lines, columns;

   if (echo == TelnetEnabled && do_echo && undrawn && End()) {
      undrawn = false;
      if (prompt) output(prompt);
      output(data, End());
      if (!AtEnd()) {			// Move cursor back to point.
         lines = EndLine() - PointLine();
         columns = EndColumn() - PointColumn();
         // XXX ANSI!
         if (lines) print("\033[%dA", lines);
         if (columns > 0) {
            print("\033[%dD", columns);
         } else if (columns < 0) {
            print("\033[%dC", -columns);
         }
      }
   }
}

inline void Telnet::beginning_of_line()	// Jump to beginning of line.
{
   int lines, columns;

   if (echo == TelnetEnabled && do_echo && Point()) {
      lines = PointLine() - StartLine();
      columns = PointColumn() - StartColumn();
      if (lines) print("\033[%dA", lines); // XXX ANSI!
      if (columns > 0) {
         print("\033[%dD", columns);	// XXX ANSI!
      } else if (columns < 0) {
         print("\033[%dC", -columns);	// XXX ANSI!
      }
   }
   point = data;
}

inline void Telnet::end_of_line()	// Jump to end of line.
{
   int lines, columns;

   if (echo == TelnetEnabled && do_echo && End() && !AtEnd()) {
      lines = EndLine() - PointLine();
      columns = EndColumn() - PointColumn();
      if (lines) print("\033[%dB", lines); // XXX ANSI!
      if (columns > 0) {
         print("\033[%dC", columns);	// XXX ANSI!
      } else if (columns < 0) {
         print("\033[%dD", -columns);	// XXX ANSI!
      }
   }
   point = free;
}

inline void Telnet::kill_line()		// Kill from point to end of line.
{
   if (!AtEnd()) {
      if (echo == TelnetEnabled && do_echo) output("\033[J"); // XXX ANSI!
      // XXX kill ring!
      free = point;			// Truncate input buffer.
      if (mark > point) mark = point;
   }
}

inline void Telnet::erase_line()	// Erase input line.
{
   beginning_of_line();
   kill_line();
}

inline void Telnet::accept_input()	// Accept input line.
{
   *free = 0;				// Make input line null-terminated.

   // If either side has Go Aheads suppressed, then the hell with it.
   // Unblock the damn output.

   if (LSGA || RSGA) {			// Unblock output.
      if (Output.head) WriteSelect();
      blocked = false;
   }

   // Jump to end of line and echo newline if necessary.
   if (echo == TelnetEnabled && do_echo) {
      if (!AtEnd()) end_of_line();
      output(Newline);
   }

   point = free = data;			// Wipe input line. (data intact)
   mark = NULL;				// Wipe mark.
   if (prompt) {			// Wipe prompt, if any.
      delete prompt;
      prompt = NULL;
   }
   prompt_len = 0;			// Wipe prompt length.

   // Call user and state-specific input line processor.

   if (InputFunc) {			// If available, call immediately.
      InputFunc(this, data);
   } else {				// Otherwise, save input line for later.
      SaveInputLine(data);
   }

   if ((end - data) > InputSize) {	// Drop buffer back to normal size.
      delete data;
      point = free = data = new char[InputSize];
      end = data + InputSize;
      mark = NULL;
   }
}

inline void Telnet::insert_char(int ch)	// Insert character at point.
{
   if (ch >= 32 && ch < Delete) {
      for (char *p = free++; p > point; p--) *p = p[-1];
      *point++ = ch;
      // Echo character if necessary.
      if (echo == TelnetEnabled && do_echo) {
         if (!AtEnd()) output("\033[@"); // XXX ANSI!
         output(ch);
      }
   } else {
      output(Bell);
   }
}

inline void Telnet::forward_char()	// Move point forward one character.
{
   if (!AtEnd()) {
      point++;				// Change point in buffer.
      if (echo == TelnetEnabled && do_echo) {
         if (PointColumn()) {		// Advance cursor on current line.
            output("\033[C");		// XXX ANSI!
         } else {			// Move to start of next screen line.
            output("\r\n");
         }
      }
   }
}

inline void Telnet::backward_char()	// Move point backward one character.
{
   if (Point()) {
      if (echo == TelnetEnabled && do_echo) {
         if (PointColumn()) {		// Backspace on current screen line.
            output(Backspace);
         } else {			// Move to end of previous screen line.
            print("\033[A\033[%dC", width - 1); // XXX ANSI!
         }
      }
      point--;				// Change point in buffer.
   }
}

inline void Telnet::erase_char()	// Erase input character before point.
{
   if (point > data) {
      point--;
      free--;
      for (char *p = point; p < free; p++) *p = p[1];
      if (echo == TelnetEnabled && do_echo) {
         if (AtEnd()) {
            output("\010 \010");	// Echo backspace, space, backspace.
         } else {
            // XXX ANSI!
            output("\010\033[P");	// Backspace, delete character.
         }
      }
   }
}

inline void Telnet::delete_char()	// Delete character at point.
{
   if (End() && !AtEnd()) {
      free--;
      for (char *p = point; p < free; p++) *p = p[1];
      if (echo == TelnetEnabled && do_echo) {
         // XXX ANSI!
         output("\033[P");		// XXX Delete character.
      }
   }
}

void Telnet::InputReady(int fd)		// Telnet stream can input data.
{
   Block *block;
   register const char *from, *from_end;
   register int n;

   n = read(fd, inbuf, BufSize);
   switch (n) {
   case -1:
      switch (errno) {
      case EINTR:
      case EWOULDBLOCK:
         break;
      case ECONNRESET:
         delete this;
         break;
      default:
         warn("Telnet::InputReady(): read(fd = %d)", fd);
         delete this;
         break;
      }
      break;
   case 0:
      delete this;
      break;
   default:
      from = inbuf;
      from_end = inbuf + n;
      while (from < from_end) {
         // Make sure there's room for more in the buffer.
         if (free >= end) {
            n = end - data;
            char *tmp = new char[n + InputSize];
            strncpy(tmp, data, n);
            point = tmp + (point - data);
            if (mark) mark = tmp + (mark - data);
            free = tmp + n;
            end = free + InputSize;
            delete data;
            data = tmp;
         }
         n = *((unsigned const char *) from);
         switch (state) {
         case TelnetIAC:
            switch (n) {
            case ShutdownCommand:
               // Shutdown request.  Not a real telnet command.

               // Acknowledge request.
               command(TelnetIAC, ShutdownCommand);

               // Initiate shutdown.
               log_message("Shutdown requested by new server in 30 seconds.");
               fdtable.announce("%c%c>>> A new server is starting.  This "
                                "server will shutdown in 30 seconds... <<<"
                                "\n%c%c", Bell, Bell, Bell, Bell);
               alarm(30);
               Shutdown = 1;
               break;
            case TelnetAbortOutput:
               // Abort all output data.
               while (Output.head) {
                  block = Output.head;
                  Output.head = block->next;
                  delete block;
               }
               Output.tail = NULL;
               state = 0;
               break;
            case TelnetAreYouThere:
               // Are we here?  Yes!  Queue confirmation to command queue,
               // to be output as soon as possible.  (Does NOT wait on a
               // Go Ahead if output is blocked!)
               command("\r\n[Yes]\r\n");
               state = 0;
               break;
            case TelnetEraseCharacter:
               // Erase last input character.
               erase_char();
               state = 0;
               break;
            case TelnetEraseLine:
               // Erase current input line.
               erase_line();
               state = 0;
               break;
            case TelnetGoAhead:
               // Unblock output.
               if (Output.head) WriteSelect();
               blocked = false;
               state = 0;
               break;
            case TelnetWill:
            case TelnetWont:
            case TelnetDo:
            case TelnetDont:
               // Options negotiation.  Remember which type.
               state = n;
               break;
            case TelnetIAC:
               // Escaped (doubled) TelnetIAC is data.
               *((unsigned char *) free++) = TelnetIAC;
               state = 0;
               break;
            default:
               // Ignore any other telnet command.
               state = 0;
               break;
            }
            from++;		// Next input character.
            break;
         case TelnetWill:
         case TelnetWont:
            // Negotiate remote option.
            switch (n) {
            case TelnetSuppressGoAhead:
               if (state == TelnetWill) {
                  RSGA |= TelnetWillWont;
                  if (!(RSGA & TelnetDoDont)) {
                     // Turn on SUPPRESS-GO-AHEAD option.
                     RSGA |= TelnetDoDont;
                     command(TelnetIAC, TelnetDo, TelnetSuppressGoAhead);

                     // Me, too!
                     if (!LSGA) set_LSGA(LSGA_callback, true);

                     // Unblock output.
                     if (Output.head) WriteSelect();
                     blocked = false;
                  }
               } else {
                  RSGA &= ~TelnetWillWont;
                  if (RSGA & TelnetDoDont) {
                     // Turn off SUPPRESS-GO-AHEAD option.
                     RSGA &= ~TelnetDoDont;
                     command(TelnetIAC, TelnetDont, TelnetSuppressGoAhead);
                  }
               }
               if (RSGA_callback) {
                  RSGA_callback(this);
                  RSGA_callback = NULL;
               }
               break;
            default:
               // Don't know this option, refuse it.
               if (state == TelnetWill) {
                  command(TelnetIAC, TelnetDont, n);
               }
               break;
            }
            state = 0;
            from++;			// Next input character.
            break;
         case TelnetDo:
         case TelnetDont:
            // Negotiate local option.
            switch (n) {
            case TelnetEcho:
               if (state == TelnetDo) {
                  echo |= TelnetDoDont;
                  if (!(echo & TelnetWillWont)) {
                     // Turn on ECHO option.
                     echo |= TelnetWillWont;
                     command(TelnetIAC, TelnetWill, TelnetEcho);
                  }
               } else {
                  echo &= ~TelnetDoDont;
                  if (echo & TelnetWillWont) {
                     // Turn off ECHO option.
                     echo &= ~TelnetWillWont;
                     command(TelnetIAC, TelnetWont, TelnetEcho);
                  }
               }
               if (echo_callback) {
                  echo_callback(this);
                  echo_callback = NULL;
               }
               break;
            case TelnetSuppressGoAhead:
               if (state == TelnetDo) {
                  LSGA |= TelnetDoDont;
                  if (!(LSGA & TelnetWillWont)) {
                     // Turn on SUPPRESS-GO-AHEAD option.
                     LSGA |= TelnetWillWont;
                     command(TelnetIAC, TelnetWill, TelnetSuppressGoAhead);

                     // You can too.
                     if (!RSGA) set_RSGA(RSGA_callback, true);

                     // Unblock output.
                     if (Output.head) WriteSelect();
                     blocked = false;
                  }
               } else {
                  LSGA &= ~TelnetDoDont;
                  if (LSGA & TelnetWillWont) {
                     // Turn off SUPPRESS-GO-AHEAD option.
                     LSGA &= ~TelnetWillWont;
                     command(TelnetIAC, TelnetWont, TelnetSuppressGoAhead);
                  }
               }
               if (LSGA_callback) {
                  LSGA_callback(this);
                  LSGA_callback = NULL;
               }
               break;
            default:
               // Don't know this option, refuse it.
               if (state == TelnetDo) {
                  command(TelnetIAC, TelnetWont, n);
               }
               break;
            }
            state = 0;
            from++;			// Next input character.
            break;
         case Return:
            // Throw away next character.
            state = 0;
            from++;			// Next input character.
            break;
         default:			// Normal data.
            state = 0;
            while (!state && from < from_end && free < end) {
               switch (n = *((unsigned char *) from++)) {
               case TelnetIAC:
                  state = TelnetIAC;
                  break;
               case ControlA:
                  beginning_of_line();
                  break;
               case ControlB:
                  backward_char();
                  break;
               case ControlD:
                  delete_char();
                  break;
               case ControlE:
                  end_of_line();
                  break;
               case ControlF:
                  forward_char();
                  break;
               case Backspace:
               case Delete:
                  erase_char();
                  break;
               case ControlK:
                  kill_line();
                  break;
               case ControlL:
                  UndrawInput();
                  RedrawInput();
                  break;
               case Return:
                  state = Return;
                  // fall through...
               case Newline:
                  accept_input();
                  break;
               default:
                  insert_char(n);
                  break;
               }
            }
            break;
         }
      }
      break;
   }
}

void Telnet::OutputReady(int fd)	// Telnet stream can output data.
{
   Block *block;
   register int n;

   // Send command data, if any.
   while (Command.head) {
      block = Command.head;
      n = write(fd, block->data, block->free - block->data);
      switch (n) {
      case -1:
         switch (errno) {
         case EINTR:
         case EWOULDBLOCK:
            return;
         default:
            warn("Telnet::OutputReady(): write(fd = %d)", fd);
            delete this;
            break;
         }
         break;
      default:
         block->data += n;
         if (block->data >= block->free) {
            if (block->next) {
               Command.head = block->next;
            } else {
               Command.head = Command.tail = NULL;
            }
            delete block;
         }
         break;
      }
   }

   // Don't write any user data if output is blocked.
   if (blocked || !Output.head) {
      NoWriteSelect();
      return;
   }

   // Send user data, if any.
   while (Output.head) {
      block = Output.head;
      n = write(fd, block->data, block->free - block->data);
      switch (n) {
      case -1:
         switch (errno) {
         case EINTR:
         case EWOULDBLOCK:
            return;
         default:
            warn("Telnet::OutputReady(): write(fd = %d)", fd);
            delete this;
            break;
         }
         break;
      default:
         block->data += n;
         if (block->data >= block->free) {
            if (block->next) {
               Output.head = block->next;
            } else {
               Output.head = Output.tail = NULL;
            }
            delete block;
         }
         break;
      }
   }

   // Done sending all queued output.
   NoWriteSelect();

   // Close connection if ready to.
   if (closing) {
      delete this;
      return;
   }

   // Do the Go Ahead thing, if we must.
   if (!LSGA) {
      command(TelnetIAC, TelnetGoAhead);

      // Only block if both sides are doing Go Aheads.
      if (!RSGA) blocked = true;
   }
}

void quit(int sig)			// received SIGQUIT or SIGTERM
{
   log_message("Shutdown requested by signal in 30 seconds.");
   fdtable.announce("%c%c>>> This server will shutdown in 30 seconds... <<<"
                    "\n%c%c", Bell, Bell, Bell, Bell);
   alarm(30);
   Shutdown = 1;
}

void alrm(int sig)			// received SIGALRM
{
   // Ignore unless shutting down.
   if (Shutdown) {
      if (Shutdown == 1) {
         log_message("Final shutdown warning.");
         fdtable.announce("%c%c>>> Server shutting down NOW!  Goodbye. <<<"
                          "\n%c%c", Bell, Bell, Bell, Bell);
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
   sessions = NULL;
   if (chdir(HOME)) error(HOME);
   OpenLog();
   FD_ZERO(&readfds);
   FD_ZERO(&writefds);
   fdtable.OpenListen(Port);

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
      // Exit if shutting down and no users are left.
      if (Shutdown && !sessions) {
         log_message("All connections closed, shutting down.");
         log_message("Server down.");
         if (logfile) fclose(logfile);
         exit(0);
      }
      fdtable.Select();
   }
}
