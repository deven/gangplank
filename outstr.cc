// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// outstr.h -- OutputStream class implementation.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Include files.
#include "outstr.h"
#include "phoenix.h"
#include "telnet.h"

void OutputStream::OutputObject::output(Telnet *telnet) // Output object.
{
   OutputObj->output(telnet);
   telnet->TimingMark();
}

void OutputStream::Enqueue(Telnet *telnet, Output *out) // Enqueue output.
{
   if (!out) return;
   if (tail) {
      tail->next = new OutputObject(out);
      tail = tail->next;
   } else {
      head = tail = new OutputObject(out);
   }
   if (telnet && telnet->acknowledge) while (SendNext(telnet)) ;
}

void OutputStream::Dequeue()		// Dequeue all acknowledged output.
{
   OutputObject *out;

   if (Acknowledged) {
      while (Acknowledged && Sent && (out = head)) {
         Acknowledged--;
         Sent--;
         head = out->next;
         delete out;
      }
      if (!head) {
         sent = tail = NULL;
         Acknowledged = Sent = 0;
      }
   }
}

bool OutputStream::SendNext(Telnet *telnet) // Send next output object.
{
   if (!telnet || (!sent && !head)) return false;
   if (sent && !sent->next) {
      telnet->RedrawInput();
      return false;
   } else {
      sent = sent ? sent->next : head;
      telnet->UndrawInput();
      sent->output(telnet);
      Sent++;
   }
   return true;
}
