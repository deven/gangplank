// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// telnet.h -- Telnet class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _TELNET_H
#define _TELNET_H 1

// Include files.
#include "fd.h"
#include "fdtable.h"
#include "outbuf.h"
#include "output.h"
#include "phoenix.h"

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
   TelnetSuppressGoAhead = 3,
   TelnetTimingMark = 6
};

// Telnet option bits.
static const int TelnetWillWont = 1;
static const int TelnetDoDont = 2;
static const int TelnetEnabled = (TelnetDoDont|TelnetWillWont);

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
   Pointer<Session> session;		// link to session object
   char *data;				// start of input data
   char *free;				// start of free area of allocated block
   const char *end;			// end of allocated block (+1)
   char *point;				// current point location
   const char *mark;			// current mark location
   char *prompt;			// current prompt
   int prompt_len;			// length of current prompt
   Pointer<Name> reply_to;		// sender of last private message
   OutputBuffer Output;			// pending data output
   OutputBuffer Command;		// pending command output
   unsigned char state;			// state (0/\r/IAC/WILL/WONT/DO/DONT)
   bool undrawn;			// input line undrawn for output?
   bool blocked;			// output blocked?
   bool closing;			// connection closing?
   bool acknowledge;			// use telnet TIMING-MARK option?
   bool DoEcho;				// should server do echo?
   char Echo;				// ECHO option (local)
   char LSGA;				// SUPPRESS-GO-AHEAD option (local)
   char RSGA;				// SUPPRESS-GO-AHEAD option (remote)
   CallbackFuncPtr Echo_callback;	// ECHO callback (local)
   CallbackFuncPtr LSGA_callback;	// SUPPRESS-GO-AHEAD callback (local)
   CallbackFuncPtr RSGA_callback;	// SUPPRESS-GO-AHEAD callback (remote)

   // Send announcement to everyone.  (Formatted write to all connections.)
   static void announce(const char *format, ...);

   // Nuke a user (force close connection).
   static void nuke(Telnet *telnet, int fd, bool drain);

   Telnet(int lfd);			// constructor
   ~Telnet();				// destructor
   void Closed();			// Connection is closed.
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
   void Close(bool drain = true);	// Close telnet connection.
   void nuke(Telnet *telnet, bool drain); // force close connection
   void output(int byte);		// queue output byte
   void output(const char *buf);	// queue output data
   void output(const char *buf, int len); // queue output data (with length)
   void print(const char *format, ...);	// formatted write
   void echo(int byte);			// echo output byte
   void echo(const char *buf);		// echo output data
   void echo(const char *buf, int len);	// echo output data (with length)
   void echo_print(const char *format, ...);	// formatted echo
   void command(const char *buf);	// queue command data
   void command(const char *buf, int len); // queue command data (with length)
   void command(int byte);		// Queue command byte.
   void command(int byte1, int byte2);	// Queue 2 command bytes.
   void command(int byte1, int byte2, int byte3); // Queue 3 command bytes.
   void TimingMark(void);		// Queue TIMING-MARK telnet option.
   void PrintMessage(OutputType type, time_t time, Name *from,
                     const char *start); // Print user message.
   void Welcome();			// Send welcome banner and login prompt.
   void UndrawInput();			// Erase input line from screen.
   void RedrawInput();			// Redraw input line on screen.
   void set_Echo(CallbackFuncPtr callback, int state); // Set local ECHO option.
   void set_LSGA(CallbackFuncPtr callback, int state); // Set local SGA option.
   void set_RSGA(CallbackFuncPtr callback, int state); // Set remote SGA option.
   void beginning_of_line();		// Jump to beginning of line.
   void end_of_line();			// Jump to end of line.
   void kill_line();			// Kill from point to end of line.
   void erase_line();			// Erase input line.
   void previous_line();		// Jump to previous line.
   void next_line();			// Jump to next line.
   void yank();				// Yank from kill-ring.
   void accept_input();			// Accept input line.
   void insert_char(int ch);		// Insert character at point.
   void forward_char();			// Move point forward one character.
   void backward_char();		// Move point backward one character.
   void erase_char();			// Erase input character before point.
   void delete_char();			// Delete character at point.
   void transpose_chars();		// Transpose characters at point.
   void InputReady(int fd);		// Telnet stream can input data.
   void OutputReady(int fd);		// Telnet stream can output data.
};

#endif // telnet.h
