// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// output.cc -- Output and derived classes, implementations.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//

// Include files.
#include "output.h"
#include "phoenix.h"
#include "session.h"
#include "telnet.h"

void Text::output(Telnet *telnet)
{
   telnet->output(text);
}

void Message::output(Telnet *telnet)
{
   // telnet->PrintMessage(Type, time, from, to, text); XXX
   telnet->PrintMessage(Type, time, from, text);
}

void EntryNotify::output(Telnet *telnet)
{
   telnet->print("*** %s has entered Phoenix! [%s] ***\n", name->name,
                 date(time, 11, 5));
}

void ExitNotify::output(Telnet *telnet)
{
   telnet->print("*** %s has left Phoenix! [%s] ***\n", name->name,
                 date(time, 11, 5));
}
