// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// phoenix.h -- General header file.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _PHOENIX_H
#define _PHOENIX_H 1

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
#include <strings.h>
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
#define SERVER_PATH "/usr/local/sbin/phoenixd"
#endif

// Home directory for server to run in.
#ifndef HOME
#define HOME "/usr/local/lib/phoenix"
#endif

// For compatibility.
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#ifndef ECONNTIMEDOUT
#define ECONNTIMEDOUT ETIMEDOUT
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
const int DefaultPort = 6789;		// TCP port to run on

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
   Semicolon = ';', Backslash = '\\', Underscore = '_', Tilde = '~',
   Delete = 127, UnquotedUnderscore = 128, CSI = 155
};

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
typedef void (Session::*InputFuncPtr)(const char *line);

// Callback function pointer type.
typedef void (Telnet::*CallbackFuncPtr)();

// Function prototypes.
const char *date(time_t clock, int start, int len);
void OpenLog();
void log_message(const char *format, ...);
void warn(const char *format, ...);
void error(const char *format, ...);
void crash(const char *format, ...);
const char *message_start(const char *line, char *sendlist, int len,
                          bool &is_explicit);
int match_name(const char *name, const char *sendlist);
void quit(int sig);
void alrm(int sig);
void RestartServer();
void ShutdownServer();
int main(int argc, char **argv);

// Global variables.
extern int Shutdown;			// shutdown flag
extern FILE *logfile;			// log file

#endif // phoenix.h
