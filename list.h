// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// list.h -- List class interface.
//
// Copyright (c) 1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _LIST_H
#define _LIST_H 1

// Include files.
#include "node.h"
#include "object.h"
#include "phoenix.h"

template <class Type> class Node;

template <class Type>
class List: public Object {
   typedef Node<Type> NodeType;
private:
   int count;
   Pointer<NodeType> head;
   Pointer<NodeType> tail;
public:
   List(): count(0) { }
   ~List() { while (Dequeue()) ; }
   int AddHead(Type *ptr);
   int AddTail(Type *ptr);
   Pointer<Type> RemHead();
   Pointer<Type> RemTail();
   Pointer<Type> Delete(NodeType *node) {
      if (!node) return NULL;
      if (!node->prev) return RemHead();
      if (!node->next) return RemTail();
      count--;
      node->prev->next = node->next;
      node->next->prev = node->prev;
      node->next = node->prev = NULL;
      return node->obj;
   }
   int Enqueue(Type *ptr) { return AddTail(ptr); }
   Pointer<Type> Dequeue() { return RemHead(); }
   int Push(Type *ptr) { return AddTail(ptr); }
   Pointer<Type> Pop() { return RemTail(); }
   int Shift(Type *ptr) { return AddHead(ptr); }
   Pointer<Type> Unshift() { return RemHead(); }
};

#endif // list.h
