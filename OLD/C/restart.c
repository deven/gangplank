/*
 * Utility program to restart conferencing server from cron.
 *
 * restart.c -- restart code.
 *
 * Copyright (c) 1993 Deven T. Corzine
 *
 */

/* Include files. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* Location of server binary. */
#ifndef SERVER_PATH
#define SERVER_PATH "/usr/local/bin/conf"
#endif

/* TCP port server runs on. */
#ifndef PORT
#define PORT 6789
#endif

int check_for_server(int port)		/* check for running server */
{
   struct sockaddr_in saddr;		/* socket address */
   int fd;				/* listening socket fd */
   int option = 1;			/* option to set for setsockopt() */

   /* Initialize listening socket. */
   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_family = AF_INET;
   saddr.sin_addr.s_addr = htonl(INADDR_ANY);
   saddr.sin_port = htons((u_short) port);
   if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) return 0;
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option))) {
      close(fd);
      return 0;
   }
   if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr))) {
      close(fd);
      return (errno == EADDRINUSE);
   }
   close(fd);
   return 0;
}

int main(int argc, char **argv)		/* main program */
{
   /* If the server is already running, silently exit. */
   if (check_for_server(PORT)) exit(0);

   /* Restart the server. */
   execl(SERVER_PATH, SERVER_PATH, NULL);

   /* Failed to execute server; print error message and exit. */
   fprintf(stderr, "%s: %s\n", SERVER_PATH, strerror(errno));
   exit(1);
}
