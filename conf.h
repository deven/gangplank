/*
 * Conferencing system server.
 *
 * conf.h -- constants, structures, variable declarations and prototypes.
 *
 * Copyright (c) 1992-1993 Deven T. Corzine
 *
 */

/* Check if previously included. */
#ifndef _CONF_H
#define _CONF_H 1

/* Include files. */
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

/* General parameters. */
#define BACKLOG 8
#define BLOCKSIZE 1024
#define BUFSIZE 32768
#define INPUTSIZE 256
#define NAMELEN 33
#define PORT 6789

/* For compatibility. */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

/* Telnet commands. */
#define COMMAND_SHUTDOWN 24		/* Not a real telnet command! */
#define TELNET_SUBNEGIOTIATION_END 240
#define TELNET_NOP 241
#define TELNET_DATA_MARK 242
#define TELNET_BREAK 243
#define TELNET_INTERRUPT_PROCESS 244
#define TELNET_ABORT_OUTPUT 245
#define TELNET_ARE_YOU_THERE 246
#define TELNET_ERASE_CHARACTER 247
#define TELNET_ERASE_LINE 248
#define TELNET_GO_AHEAD 249
#define TELNET_SUBNEGIOTIATION_BEGIN 250
#define TELNET_WILL 251
#define TELNET_WONT 252
#define TELNET_DO 253
#define TELNET_DONT 254
#define TELNET_IAC 255

/* Telnet options. */
#define TELNET_ECHO 1
#define TELNET_SUPPRESS_GO_AHEAD 3

/* Telnet option bits. */
#define TELNET_WILL_WONT 1
#define TELNET_DO_DONT 2
#define TELNET_ENABLED (TELNET_DO_DONT|TELNET_WILL_WONT)

/* Option states. */
#define ON 1
#define OFF 0

/* Forward declaration. */
struct telnet;

/* Input function pointer type. */
typedef void (*input_func_ptr)(struct telnet *telnet, const char *line);

/* Callback function pointer type. */
typedef void (*callback_func_ptr)(struct telnet *telnet);

/* Input buffer consisting of a single buffer, resized as needed. */
struct InputBuffer {
   char *data;				/* start of input data */
   char *free;				/* start of allocated block free area */
   const char *end;			/* end of allocated block (+1) */
};

/* Single input lines waiting to be processed. */
struct Line {
   const char *line;			/* input line */
   struct Line *next;			/* next input line */
};

/* Output buffer consisting of linked list of output blocks. */
struct OutputBuffer {
   struct block *head;			/* first data block */
   struct block *tail;			/* last data block */
};

/* Block in a data buffer, allocated with data immediately following. */
struct block {
   char *data;				/* start of data, not allocated block */
   char *free;				/* start of free area */
   const char *end;			/* end of allocated block (+1) */
   struct block *next;			/* next block in data buffer */
   /* data follows contiguously */
};

/* Telnet options are stored in a single byte each, with bit 0 representing
   WILL or WON'T state and bit 1 representing DO or DON'T state.  The option
   is only enabled when both bits are set. */

/* Data about a particular telnet connection. */
struct telnet {
   struct telnet *next;			/* next telnet connection */
   int fd;				/* file descriptor for TCP connection */
   struct user *user;			/* link to user structure */
   struct InputBuffer input;		/* pending input */
   struct Line *lines;			/* unprocessed input lines */
   struct OutputBuffer output;		/* pending data output */
   struct OutputBuffer command;		/* pending command output */
   unsigned char state;			/* state (0/\r/IAC/WILL/WONT/DO/DONT) */
   char undrawn;			/* input line undrawn for output? */
   char blocked;			/* output blocked? (boolean) */
   char closing;			/* connection closing? (boolean) */
   char do_echo;			/* should server do echo? (boolean) */
   char echo;				/* ECHO option (local) */
   char LSGA;				/* SUPPRESS-GO-AHEAD option (local) */
   char RSGA;				/* SUPPRESS-GO-AHEAD option (remote) */
   callback_func_ptr echo_callback;	/* ECHO callback (local) */
   callback_func_ptr LSGA_callback;	/* local SUPPRESS-GO-AHEAD callback */
   callback_func_ptr RSGA_callback;	/* remote SUPPRESS-GO-AHEAD callback */
};

/* XXX need session structure? */

/* Data about a particular user. */
struct user {
   struct telnet *telnet;		/* telnet connection for this user */
   input_func_ptr input;		/* input processor function pointer */
   /* XXX change! vvv  */
   char user[32];			/* account name */
   char passwd[32];			/* password for this account */
   /* XXX change! ^^^ */
   char name[NAMELEN];			/* current user name (pseudo) */
   char last_sendlist[32];		/* last explicit sendlist */
   time_t login_time;			/* time logged in */
   time_t idle_since;			/* last idle time */
};

/* Function prototypes. */
const char *date(time_t clock, int start, int len);
void log_message(const char *format, ...);
void warn(const char *format, ...);
void error(const char *format, ...);
void *alloc(int len);
struct block *alloc_block(void);
void free_block(struct block *block);
void free_user(struct user *user);
void save_input_line(struct telnet *telnet, const char *line);
void set_input_function(struct telnet *telnet, input_func_ptr input);
void output(struct telnet *telnet, const char *buf);
void print(struct telnet *telnet, const char *format, ...);
void announce(const char *format, ...);
void put_command(struct telnet *telnet, int cmd);
const char *message_start(const char *line, char *sendlist, int len);
int match_name(const char *name, const char *sendlist);
void echo(struct telnet *telnet, callback_func_ptr callback, int state);
void LSGA(struct telnet *telnet, callback_func_ptr callback, int state);
void RSGA(struct telnet *telnet, callback_func_ptr callback, int state);
void request_shutdown(int port);
int listen_on(int port, int backlog);
void welcome(struct telnet *telnet);
void login(struct telnet *telnet, const char *line);
void password(struct telnet *telnet, const char *line);
void name(struct telnet *telnet, const char *line);
void process_input(struct telnet *telnet, const char *line);
void who_cmd(struct telnet *telnet);
void new_connection(int lfd);
void close_connection(struct telnet *telnet);
void undraw_line(struct telnet *telnet);
void redraw_line(struct telnet *telnet);
void erase_character(struct telnet *telnet);
void erase_line(struct telnet *telnet);
void input_ready(struct telnet *telnet);
void output_ready(struct telnet *telnet);
void quit(int sig);
void alrm(int sig);
int main(int argc, char **argv);

/* Global variables. */
extern struct telnet *connections;	/* telnet connections */
extern int shutdown_flag;		/* shutdown flag */
extern int nfds;			/* number of fds available */
extern fd_set readfds;			/* read fdset for select() */
extern fd_set writefds;			/* write fdset for select() */
extern FILE *logfile;			/* log file */

#endif /* conf.h */
