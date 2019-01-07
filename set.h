// -*- C++ -*-
//
// Phoenix conferencing system server.
//
// set.h -- Set class interface & implementation.
//
// Copyright (c) 1994 Deven T. Corzine
//

// Check if previously included.
#ifndef _SET_H
#define _SET_H 1

// Include files.
#include "list.h"
#include "object.h"
#include "phoenix.h"

template <class Type> class SetIter;

template <class Type>
class Set: public Object {
friend class SetIter<Type>;
private:
   List<Type> l;
public:
   int Count() { return l.Count(); }
   int In(Pointer<Type> &ptr);
   void Add(Pointer<Type> &ptr);
   void Remove(Pointer<Type> &ptr);
};

template <class Type>
int Set<Type>::In(Pointer<Type> &ptr) {
   ListIter<Type> i(l);
   while (i++) if (ptr == i) return true;
   return false;
}

template <class Type>
void Set<Type>::Add(Pointer<Type> &ptr) {
   if (!In(ptr)) l.AddTail(ptr);
}

template <class Type>
void Set<Type>::Remove(Pointer<Type> &ptr) {
   ListIter<Type> i(l);
   while (i++) if (ptr == i) i.Remove();
}

template <class Type>
class SetIter {
private:
   typedef Set<Type> SetType;
   ListIter<Type> iter;
public:
   SetIter() { }
   SetIter(SetType &s): iter(s.l) { }
   SetIter(Pointer<SetType> &s): iter(s->l) { }
   SetIter &operator =(SetType &s) { iter = s.l; }
   SetIter &operator =(Pointer<SetType> &s) { iter = s->l; }
   Type *operator ->() { return iter; }
   operator Type *() { return iter; }
   operator int() { return iter; }
   Type *operator --() { return --iter; }
   Type *operator ++() { return ++iter; }
   Type *operator --(int) { return iter--; }
   Type *operator ++(int) { return iter++; }
   Pointer<Type> Remove() { return iter.Remove(); }
};

#endif // set.h
