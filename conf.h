// -*- C++ -*-
//
// Conferencing system server.
//
// conf.h -- constants, structures, variable declarations and prototypes.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _CONF_H
#define _CONF_H 1

// Include files.
extern "C" {
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
};

// Location of server binary.
#ifndef SERVER_PATH
#define SERVER_PATH "/usr/local/bin/conf"
#endif

// Home directory for server to run in.
#ifndef HOME
#define HOME "/usr/local/lib/conf"
#endif

// For compatibility.
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

// Declare strerror() if needed.
#ifdef NEED_STRERROR
const char *strerror(int err)
#endif

// General parameters.
const int BlockSize = 1024;		// data size for block
const int BufSize = 32768;		// general temporary buffer size
const int InputSize = 256;		// default size of input line buffer
const int NameLen = 33;			// maximum length of name (with null)
const int SendlistLen = 33;		// maximum length of sendlist (w/null)
const int Port = 6789;			// TCP port to run on

// Boolean type.
#ifdef NO_BOOLEAN
#define bool int
#define false (0)
#define true (1)
#endif

// Character codes.
enum Char {
   Null, ControlA, ControlB, ControlC, ControlD, ControlE, ControlF, ControlG,
   ControlH, ControlI, ControlJ, ControlK, ControlL, ControlM, ControlN,
   ControlO, ControlP, ControlQ, ControlR, ControlS, ControlT, ControlU,
   ControlV, ControlW, ControlX, ControlY, ControlZ, Escape,
   Bell = '\007', Backspace = '\010', Tab = '\t', Linefeed = '\n',
   Newline = '\n', Return = '\r', Space = ' ', Quote = '\"', Colon = ':',
   Semicolon = ';', Backslash = '\\', Underscore = '_', Delete = 127,
   UnquotedUnderscore = 128		// unquoted underscore character in name
};

// Types of FD subclasses.
enum FDType {UnknownFD, ListenFD, TelnetFD};

// Telnet commands.
enum TelnetCommand {
   ShutdownCommand = 24,		// Not a real telnet command!
   TelnetSubnegotiationEnd = 240,
   TelnetNOP = 241,
   TelnetDataMark = 242,
   TelnetBreak = 243,
   TelnetInterruptProcess = 244,
   TelnetAbortOutput = 245,
   TelnetAreYouThere = 246,
   TelnetEraseCharacter = 247,
   TelnetEraseLine = 248,
   TelnetGoAhead = 249,
   TelnetSubnegotiationBegin = 250,
   TelnetWill = 251,
   TelnetWont = 252,
   TelnetDo = 253,
   TelnetDont = 254,
   TelnetIAC = 255
};

// Telnet options.
enum TelnetOption {
   TelnetEcho = 1,
   TelnetSuppressGoAhead = 3
};

// Telnet option bits.
const int TelnetWillWont = 1;
const int TelnetDoDont = 2;
const int TelnetEnabled = (TelnetDoDont|TelnetWillWont);

// Class declarations.
class Block;
class FD;
class FDTable;
class Line;
class Listen;
class OutputBuffer;
class Session;
class Telnet;
class User;

// Input function pointer type.
typedef void (*InputFuncPtr)(Telnet *telnet, const char *line);

// Callback function pointer type.
typedef void (*CallbackFuncPtr)(Telnet *telnet);

// Function prototypes.
const char *date(time_t clock, int start, int len);
void OpenLog();
void log_message(const char *format, ...);
void warn(const char *format, ...);
void error(const char *format, ...);
void notify(const char *format, ...);
const char *message_start(const char *line, char *sendlist, int len,
                          int *is_explicit);
int match_name(const char *name, const char *sendlist);
void welcome(Telnet *telnet);
void login(Telnet *telnet, const char *line);
void password(Telnet *telnet, const char *line);
void name(Telnet *telnet, const char *line);
void process_input(Telnet *telnet, const char *line);
void who_cmd(Telnet *telnet);
void erase_line(Telnet *telnet);
void quit(int sig);
void alrm(int sig);
int main(int argc, char **argv);

// Global variables.
extern Session *sessions;		// active sessions
extern int Shutdown;			// shutdown flag
extern FDTable fdtable;			// File descriptor table.
extern fd_set readfds;			// read fdset for select()
extern fd_set writefds;			// write fdset for select()
extern FILE *logfile;			// log file

// Single input lines waiting to be processed.
class Line {
public:
   const char *line;			// input line
   Line *next;				// next input line

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

// Block in a data buffer.
class Block {
public:
   Block *next;				// next block in data buffer
   const char *data;			// start of data, not allocated block
   char *free;				// start of free area
   char block[BlockSize];		// actual data block

   Block() {				// constructor
      next = NULL;
      data = free = block;
   }
};

// Output buffer consisting of linked list of output blocks.
class OutputBuffer {
public:
   Block *head;				// first data block
   Block *tail;				// last data block

   OutputBuffer() {			// constructor
      head = tail = NULL;
   }
   ~OutputBuffer() {			// destructor
      Block *block;

      while (head) {			// Free any remaining blocks in queue.
         block = head;
         head = block->next;
         delete block;
      }
      tail = NULL;
   }
   int out(int byte) {			// Output one byte.
      int select;

      if ((select = !tail)) {
         head = tail = new Block;
      } else if (tail->free >= tail->block + BlockSize) {
         tail->next = new Block;
         tail = tail->next;
      }
      *tail->free++ = byte;
      return select;
   }
   int out(int byte1, int byte2) {	 // Output two bytes.
      int select;

      if ((select = !tail)) {
         head = tail = new Block;
      } else if (tail->free >= tail->block + BlockSize - 1) {
         tail->next = new Block;
         tail = tail->next;
      }
      *tail->free++ = byte1;
      *tail->free++ = byte2;
      return select;
   }
   int out(int byte1, int byte2, int byte3) { // Output three bytes.
      int select;

      if ((select = !tail)) {
         head = tail = new Block;
      } else if (tail->free >= tail->block + BlockSize - 2) {
         tail->next = new Block;
         tail = tail->next;
      }
      *tail->free++ = byte1;
      *tail->free++ = byte2;
      *tail->free++ = byte3;
      return select;
   }
};

// Data about a particular session.
class Session {
public:
   Session *next;			// next session
   User *user;				// user this session belongs to
   Telnet *telnet;			// telnet connection for this session
   time_t login_time;			// time logged in
   time_t idle_since;			// last idle time
   char name[NameLen];			// current user name (pseudo)
   char default_sendlist[SendlistLen];	// current default sendlist
   char last_sendlist[SendlistLen];	// last explicit sendlist

   Session(Telnet *t);			// constructor
   ~Session();				// destructor
};

// Data about a particular user.
class User {
public:
   int priv;				// privilege level
   // XXX change! vvv
   char user[32];			// account name
   char password[32];			// password for this account
   // XXX change! ^^^
   char reserved_name[NameLen];		// reserved user name (pseudo)

   User(Session *s);			// constructor
};

// Data about a particular file descriptor.
class FD {
public:
   FDType type;				// type of file descriptor
   int fd;				// file descriptor

   virtual void InputReady(int fd) = 0;	// Input ready on file descriptor fd.
   virtual void OutputReady(int fd) = 0; // Output ready on file descriptor fd.
   virtual void output(const char *buf) {} // queue output data
   virtual ~FD() {}			// destructor
   void NonBlocking() {			// Place fd in non-blocking mode.
      int flags;

      if ((flags = fcntl(fd, F_GETFL)) < 0) {
         error("FD::NonBlocking(): fcntl(F_GETFL)");
      }
      flags |= O_NONBLOCK;
      if (fcntl(fd, F_SETFL, flags) == -1) {
         error("FD::NonBlocking(): fcntl(F_SETFL)");
      }
   }
   void ReadSelect() {			// Select fd for reading.
      FD_SET(fd, &readfds);
   }
   void NoReadSelect() {		// Do not select fd for reading.
      FD_CLR(fd, &readfds);
   }
   void WriteSelect() {			// Select fd for writing.
      FD_SET(fd, &writefds);
   }
   void NoWriteSelect() {		// Do not select fd for writing.
      FD_CLR(fd, &writefds);
   }
};

// File descriptor table.
class FDTable {
protected:
   FD **array;				// dynamic array of file descriptors
   int size;				// size of file descriptor table
   int used;				// number of file descriptors used
public:
   FDTable();				// constructor
   ~FDTable();				// destructor
   void OpenListen(int port);		// Open a listening port.
   void OpenTelnet(int lfd);		// Open a telnet connection.
   void Close(int fd);			// Close fd.
   void Select();			// Select across all ready connections.
   void InputReady(int fd);		// Input ready on file descriptor fd.
   void OutputReady(int fd);		// Output ready on file descriptor fd.

   // Send announcement to everyone.  (Formatted write to all connections.)
   void announce(const char *format, ...);

   // Nuke a user (force close connection).
   void nuke(Telnet *telnet, int fd, int drain);

   // Send private message by fd #.
   void SendByFD(Telnet *telnet, int fd, const char *sendlist, int is_explicit,
                 const char *msg);

   // Send public message to everyone.
   void SendEveryone(Telnet *telnet, const char *msg);

   // Send private message by partial name match.
   void SendPrivate(Telnet *telnet, const char *sendlist, int is_explicit,
                    const char *msg);
};

// Listening socket (subclass of FD).
class Listen: public FD {
protected:
   void RequestShutdown(int port);	// Connect to port, request shutdown.
public:
   Listen(int port);			// constructor
   ~Listen() {				// destructor
      if (fd != -1) close(fd);
   }
   void InputReady(int fd) {		// Input ready on file descriptor fd.
      fdtable.OpenTelnet(fd);		// Accept pending telnet connection.
   }
   void OutputReady(int fd) {		// Output ready on file descriptor fd.
      error("Listen::OutputReady(fd = %d): invalid operation!", fd);
   }
};

// Telnet options are stored in a single byte each, with bit 0 representing
// WILL or WON'T state and bit 1 representing DO or DON'T state.  The option
// is only enabled when both bits are set.

// Data about a particular telnet connection (subclass of FD).
class Telnet: public FD {
protected:
   void LogCaller();			// Log calling host and port.
public:
   static const int width = 80;		// XXX Hardcoded screen width
   static const int height = 24;	// XXX Hardcoded screen height
   Session *session;			// link to session object
   char *data;				// start of input data
   char *free;				// start of free area of allocated block
   const char *end;			// end of allocated block (+1)
   char *point;				// current point location
   const char *mark;			// current mark location
   char *prompt;			// current prompt
   int prompt_len;			// length of current prompt
   Line *lines;				// unprocessed input lines
   OutputBuffer Output;			// pending data output
   OutputBuffer Command;		// pending command output
   InputFuncPtr InputFunc;		// input processor function
   unsigned char state;			// state (0/\r/IAC/WILL/WONT/DO/DONT)
   bool undrawn;			// input line undrawn for output?
   bool blocked;			// output blocked?
   bool closing;			// connection closing?
   bool do_echo;			// should server do echo?
   char echo;				// ECHO option (local)
   char LSGA;				// SUPPRESS-GO-AHEAD option (local)
   char RSGA;				// SUPPRESS-GO-AHEAD option (remote)
   CallbackFuncPtr echo_callback;	// ECHO callback (local)
   CallbackFuncPtr LSGA_callback;	// SUPPRESS-GO-AHEAD callback (local)
   CallbackFuncPtr RSGA_callback;	// SUPPRESS-GO-AHEAD callback (remote)

   Telnet(int lfd);			// constructor
   ~Telnet();				// destructor
   void Prompt(const char *p);		// Print and set new prompt.
   bool AtEnd() { return point == free; } // point at end of input?
   int Start() { return prompt_len; }	// start of input (after prompt)
   int StartLine() { return Start() / width; } // start of input line
   int StartColumn() { return Start() % width; } // start of input column
   int Point() { return point - data; }	// current point (input cursor position)
   int PointLine() { return (Start() + Point()) / width; } // point line
   int PointColumn() { return (Start() + Point()) % width; } // point column
   int Mark() { return mark - data; }	// mark position (saved cursor)
   int MarkLine() { return (Start() + Mark()) / width; } // mark line
   int MarkColumn() { return (Start() + Mark()) % width; } // mark column
   int End() { return free - data; }	// end of input
   int EndLine() { return (Start() + End()) / width; } // end of input line
   int EndColumn() { return (Start() + End()) % width; } // end of input column
   void Close() { fdtable.Close(fd); }	// Close telnet connection.
   void nuke(Telnet *telnet, int drain); // force close connection
   void Drain();			// Drain connection, then close.
   void SaveInputLine(const char *line); // Save input line for later.
   void SetInputFunction(InputFuncPtr input); // Set user input function.
   void output(int byte);		// queue output byte
   void output(const char *buf);	// queue output data
   void output(const char *buf, int len); // queue output data (with length)
   void print(const char *format, ...);	// formatted write
   void command(const char *buf);	// queue command data
   void command(const char *buf, int len); // queue command data (with length)
   void command(int byte);		// Queue command byte.
   void command(int byte1, int byte2);	// Queue 2 command bytes.
   void command(int byte1, int byte2, int byte3); // Queue 3 command bytes.
   void UndrawInput();			// Erase input line from screen.
   void RedrawInput();			// Redraw input line on screen.
   void OutputWithRedraw(const char *buf); // queue output w/redraw
   void PrintWithRedraw(const char *format, ...); // format output w/redraw
   void set_echo(CallbackFuncPtr callback, int state); // Set local ECHO option.
   void set_LSGA(CallbackFuncPtr callback, int state); // Set local SGA option.
   void set_RSGA(CallbackFuncPtr callback, int state); // Set remote SGA option.
   void beginning_of_line();		// Jump to beginning of line.
   void end_of_line();			// Jump to end of line.
   void kill_line();			// Kill from point to end of line.
   void erase_line();			// Erase input line.
   void accept_input();			// Accept input line.
   void insert_char(int ch);		// Insert character at point.
   void forward_char();			// Move point forward one character.
   void backward_char();		// Move point backward one character.
   void erase_char();			// Erase input character before point.
   void delete_char();			// Delete character at point.
   void InputReady(int fd);		// Telnet stream can input data.
   void OutputReady(int fd);		// Telnet stream can output data.
};

#endif // conf.h
