// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// outstr.h -- OutputStream class implementation.
//
// Copyright (c) 1992-1993 Deven T. Corzine
//

// Include files.
#include "outstr.h"
#include "phoenix.h"
#include "telnet.h"

void OutputStream::Enqueue(Telnet *telnet, Output *out) // Enqueue output.
{
   if (!out) return;
   if (tail) {
      tail->next = new OutputObject(out);
      tail = tail->next;
   } else {
      head = tail = new OutputObject(out);
      if (!telnet) return;
      telnet->UndrawInput();
      head->output->output(telnet);
   }
}

// Dequeue completed output object, then output next or redraw input.
void OutputStream::Dequeue(Telnet *telnet)
{
   if (head) {
      OutputObject *out = head;
      head = out->next;
      delete out;
      if (!head) tail = NULL;
   }
   if (!telnet) return;
   if (head) {
      head->output->output(telnet);
   } else {
      telnet->RedrawInput();
   }
}
