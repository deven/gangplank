// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// node.h -- Node class interface.
//
// Copyright (c) 1992-1994 Deven T. Corzine
//
// -*- C++ -*-
//

// Check if previously included.
#ifndef _NODE_H
#define _NODE_H 1

// Include files.
#include "list.h"
#include "object.h"
#include "phoenix.h"

template <class Type>
class Node: public Object {
friend class List<Type>;
private:
   Pointer<Node> next;			// Next node.
   Pointer<Node> prev;			// Previous node.
   Pointer<Type> obj;			// Object this node refers to.
   Node(Type *ptr): obj(ptr) { }
};

#endif // node.h
