// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// outstr.h -- OutputStream class interface.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Check if previously included.
#ifndef _OUTSTR_H
#define _OUTSTR_H 1

// Include files.
#include "output.h"
#include "phoenix.h"

class OutputStream {
private:
   class OutputObject {
   public:
      OutputObject *next;
      Output *OutputObj;

      OutputObject(Output *out) {	// constructor
	 next = NULL;
	 if ((OutputObj = out)) OutputObj->RefCnt++;
      }
      ~OutputObject() {			// destructor
	 if (OutputObj && --OutputObj->RefCnt == 0) delete OutputObj;
      }
      void output(Telnet *telnet);
   };
public:
   OutputObject *head;			// first output object
   OutputObject *sent;			// next output object to send
   OutputObject *tail;			// last output object
   int Acknowledged;			// count of acknowledged queue objects
   int Sent;				// count of sent queue objects

   OutputStream() {			// constructor
      head = sent = tail = NULL;
      Acknowledged = Sent = 0;
   }
   ~OutputStream() {			// destructor
      while (head) {			// Free any remaining output in queue.
         OutputObject *out = head;
         head = out->next;
         delete out;
      }
      sent = tail = NULL;
      Acknowledged = Sent = 0;
   }
   void Acknowledge(void) {		// Acknowledge a block of output.
      if (Acknowledged < Sent) Acknowledged++;
   }
   void Enqueue(Telnet *telnet, Output *out);
   void Dequeue(void);
   bool SendNext(Telnet *telnet);
};

#endif // outstr.h
