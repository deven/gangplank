// -*- C++ -*-
//
// Conferencing system server.
//
// outbuf.h -- OutputBuffer class interface.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _OUTBUF_H
#define _OUTBUF_H 1

// Include files.
#include "block.h"
#include "conf.h"

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
   int out(int byte1, int byte2) {	// Output two bytes.
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

#endif // outbuf.h
