// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// outbuf.h -- OutputBuffer class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _OUTBUF_H
#define _OUTBUF_H 1

// Include files.
#include "block.h"
#include "phoenix.h"

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
   char *GetData() {			// Save buffer in string and erase.
      int len = 0;
      Block *block;
      for (block = head; block; block = block->next) {
         len += block->free - block->data;
      }
      if (!len) return NULL;
      char *buf = new char[++len];
      char *p;
      for (p = buf; head; p += len) {
         block = head;
         head = block->next;
         len = block->free - block->data;
         strncpy(p, block->data, len);
         delete block;
      }
      tail = NULL;
      *p = 0;
      return buf;
   }
   bool out(int byte) {			// Output one byte.
      bool select;

      if ((select = !tail)) {
         head = tail = new Block;
      } else if (tail->free >= tail->block + BlockSize) {
         tail->next = new Block;
         tail = tail->next;
      }
      *tail->free++ = byte;
      return select;
   }
   bool out(int byte1, int byte2) {	// Output two bytes.
      bool select;

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
   bool out(int byte1, int byte2, int byte3) { // Output three bytes.
      bool select;

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

#endif // outbuf.h
