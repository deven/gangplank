// -*- C++ -*-
//
// Conferencing system server.
//
// block.h -- Block class interface.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _BLOCK_H
#define _BLOCK_H 1

// Include files.
#include "conf.h"

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

#endif // block.h
