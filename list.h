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
#include "object.h"
#include "phoenix.h"

template <class Type> class List;
template <class Type> class ListIter;

template <class Type>
class ListNode: public Object {
friend class List<Type>;
friend class ListIter<Type>;
private:
   Pointer<ListNode> next;		// Next node.
   Pointer<ListNode> prev;		// Previous node.
   Pointer<Type> obj;			// Object this node refers to.
   ListNode(Type *ptr): obj(ptr) { }
};

template <class Type>
class List: public Object {
friend class ListIter<Type>;
private:
   typedef ListNode<Type> NodeType;
   int count;
   Pointer<NodeType> head;
   Pointer<NodeType> tail;
public:
   List(): count(0) { }
   ~List() { while (Dequeue()) ; }
   int Count() { return count; }
   int AddHead(Type *ptr);
   int AddTail(Type *ptr);
   Pointer<Type> RemHead();
   Pointer<Type> RemTail();
   int Enqueue(Type *ptr) { return AddTail(ptr); }
   Pointer<Type> Dequeue() { return RemHead(); }
   int Push(Type *ptr) { return AddTail(ptr); }
   Pointer<Type> Pop() { return RemTail(); }
   int Shift(Type *ptr) { return AddHead(ptr); }
   Pointer<Type> Unshift() { return RemHead(); }
};

template <class Type>
int List<Type>::AddHead(Type *ptr) {
   Pointer<NodeType> node(new NodeType(ptr));
   node->next = head;
   if (head) {
      head->prev = node;
   } else {
      tail = node;
   }
   head = node;
   return ++count;
}

template <class Type>
int List<Type>::AddTail(Type *ptr) {
   Pointer<NodeType> node(new NodeType(ptr));
   node->prev = tail;
   if (tail) {
      tail->next = node;
   } else {
      head = node;
   }
   tail = node;
   return ++count;
}

template <class Type>
Pointer<Type> List<Type>::RemHead() {
   if (!head) return Pointer<Type>();
   Pointer<NodeType> node(head);
   count--;
   head = node->next;
   if (head) {
      head->prev = NULL;
   } else {
      tail = NULL;
   }
   node->next = node->prev = NULL;
   return node->obj;
}

template <class Type>
Pointer<Type> List<Type>::RemTail() {
   if (!tail) return Pointer<Type>();
   Pointer<NodeType> node(tail);
   count--;
   tail = node->prev;
   if (tail) {
      tail->next = NULL;
   } else {
      head = NULL;
   }
   node->next = node->prev = NULL;
   return node->obj;
}

template <class Type>
class ListIter {
private:
   typedef ListNode<Type> NodeType;
   typedef List<Type> ListType;
   Pointer<NodeType> ptr;
   Pointer<ListType> list;
public:
   ListIter() { }
   ListIter(ListType &l): list(&l) { }
   ListIter(ListType *l): list(l) { }
   ListIter &operator =(ListType &l) { list = &l; ptr = NULL; }
   ListIter &operator =(ListType *l) { list = l; ptr = NULL; }
   Type *operator ->() { return ptr ? ptr->obj : NULL; }
   operator Type *()   { return ptr ? ptr->obj : NULL; }
   operator int() { return ptr != NULL; }
   Type *operator --() { ptr = ptr ? ptr->prev : list->tail; return *this; }
   Type *operator ++() { ptr = ptr ? ptr->next : list->head; return *this; }
   Type *operator --(int) { ListIter<Type> *t = this; this->operator --(); return *t; }
   Type *operator ++(int) { ListIter<Type> *t = this; this->operator ++(); return *t; }
   Pointer<Type> Remove();
   int InsertBefore(Type *obj);
   int InsertAfter(Type *obj);
};

template <class Type>
Pointer<Type> ListIter<Type>::Remove() {
   if (!ptr) return Pointer<Type>();
   if (!ptr->prev) return list->RemHead();
   if (!ptr->next) return list->RemTail();
   Pointer<NodeType> node(ptr);
   ptr = ptr->next;
   list->count--;
   node->prev->next = node->next;
   node->next->prev = node->prev;
   node->next = node->prev = NULL;
   return node->obj;
}

template <class Type>
int ListIter<Type>::InsertBefore(Type *obj) {
   if (!ptr || !ptr->prev) return list->AddHead(obj);
   Pointer<NodeType> node(new NodeType(obj));
   node->next = ptr;
   node->prev = ptr->prev;
   ptr->prev->next = node;
   ptr->prev = node;
   ptr = node;
   return ++list->count;
}

template <class Type>
int ListIter<Type>::InsertAfter(Type *obj) {
   if (!ptr || !ptr->next) return list->AddTail(obj);
   Pointer<NodeType> node(new NodeType(obj));
   node->prev = ptr;
   node->next = ptr->next;
   ptr->next->prev = node;
   ptr->next = node;
   ptr = node;
   return ++list->count;
}

#endif // list.h
