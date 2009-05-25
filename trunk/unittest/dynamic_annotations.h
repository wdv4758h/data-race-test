/*
  This file is part of Valgrind, a dynamic binary instrumentation
  framework.

  Copyright (C) 2008-2008 Google Inc
     opensource@google.com 

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307, USA.

  The GNU General Public License is contained in the file COPYING.
*/

// Author: Konstantin Serebryany <opensource@google.com> 
//
// This file defines dynamic annotations for use with dynamic analysis 
// tool such as valgrind, PIN, etc. 
//
// Dynamic annotation is a source code annotation that affects 
// the generated code (that is, the annotation is not a comment). 
// Each such annotation is attached to a particular
// instruction and/or to a particular object (address) in the program.
//
// The annotations that should be used by users are macros 
// (e.g. ANNOTATE_NEW_MEMORY). 
//
// Actual implementation of these macros may differ depending on the 
// dynamic analysis tool being used. 
//
// This file supports the following dynamic analysis tools: 
// - None (DYNAMIC_ANNOTATIONS is not defined). 
//    Macros are defined empty. 
// - Helgrind (DYNAMIC_ANNOTATIONS is defined). 
//    Macros are defined as calls to non-inlinable empty functions 
//    that are intercepted by helgrind (starting from version TODO). 
//
//
// To link your program with annotations enabled, add a new file 
// with the following lines to your program: 
//    #define DYNAMIC_ANNOTATIONS_HERE
//    #include "dynamic_annotations.h"  
//
//
// TODO: using __attribute__((noinline)) might be simpler, 
// but it is less portable. 
//
#ifndef DYNAMIC_ANNOTATIONS_H__
#define DYNAMIC_ANNOTATIONS_H__

#include <unistd.h>

#ifdef DYNAMIC_ANNOTATIONS
// Annotations are enabled. 
  #ifdef DYNAMIC_ANNOTATIONS_HERE
    // Return true if the program is running under valgrind, false otherwise.
    extern "C" bool RunningOnValgrind();
    // Actually define the annotations as functions with empty body. 
    #define ANNOTATION(name, arglist...) \
      extern "C" void name (const char *file, int line, arglist) {}
  #else
    // If the program is run under valgrind, this call will be intercepted 
    // and non-zero value will be returned. 
    extern "C" bool RunningOnValgrind() { return 0; }
    // Just declare the functions. 
    #define ANNOTATION(name, arglist...) \
      extern "C" void name (const char *file, int line, arglist); 
  #endif  // DYNAMIC_ANNOTATIONS_HERE
#else  // !DYNAMIC_ANNOTATIONS
  extern "C" bool RunningOnValgrind() { return 0; }
  // Annotations are disabled. Define an empty inlinable function. 
  #define ANNOTATION(name, arglist...) \
    static inline void name (const char *file, int line, arglist) {}
#endif


ANNOTATION(AnnotateRWLockCreate, void *lock);
ANNOTATION(AnnotateRWLockDestroy, void *lock);
ANNOTATION(AnnotateRWLockAcquired, void *lock, long is_w);
ANNOTATION(AnnotateRWLockReleased, void *lock, long is_w);
ANNOTATION(AnnotateCondVarWait,     void *cv, void *lock);
ANNOTATION(AnnotateCondVarSignal,   void *cv);
ANNOTATION(AnnotateCondVarSignalAll,void *cv);
ANNOTATION(AnnotatePCQCreate,   void *pcq);
ANNOTATION(AnnotatePCQDestroy,  void *pcq);
ANNOTATION(AnnotatePCQPut,      void *pcq);
ANNOTATION(AnnotatePCQGet,      void *pcq);
ANNOTATION(AnnotateNewMemory, void *mem, long size);
ANNOTATION(AnnotateExpectRace, void *mem, const char *description);
ANNOTATION(AnnotateBenignRace, void *mem, const char *description);
ANNOTATION(AnnotateTraceMemory, void *mem);
ANNOTATION(AnnotateMutexIsUsedAsCondVar, void *mu);
ANNOTATION(AnnotateIgnoreReadsBegin, void *);
ANNOTATION(AnnotateIgnoreReadsEnd, void *);
ANNOTATION(AnnotateIgnoreWritesBegin, void *);
ANNOTATION(AnnotateIgnoreWritesEnd, void *);
ANNOTATION(AnnotatePrintMemoryUsage, long);
ANNOTATION(AnnotatePrintStats, void *);
ANNOTATION(AnnotateResetStats, void *);
ANNOTATION(AnnotatePublishMemoryRange, void *, long);
ANNOTATION(AnnotateThreadName, const char *thread_name);

ANNOTATION(AnnotateNoOp, void *arg);

// Insert right after the lock is created. 
#define ANNOTATE_RWLOCK_CREATE(lock) \
           AnnotateRWLockCreate(__FILE__, __LINE__, lock)

// Insert right before the lock is destroyed. 
#define ANNOTATE_RWLOCK_DESTROY(lock) \
           AnnotateRWLockDestroy(__FILE__, __LINE__, lock)

// Insert right after the point were 'lock' is acquired.
// Set is_w=1 for write lock, is_w=0 for reader lock. 
#define ANNOTATE_RWLOCK_ACQUIRED(lock, is_w) \
            AnnotateRWLockAcquired(__FILE__, __LINE__, lock, is_w)

// Insert right before the point where 'lock' is released. 
#define ANNOTATE_RWLOCK_RELEASED(lock, is_w) \
            AnnotateRWLockReleased(__FILE__, __LINE__, lock, is_w)

// Insert right after the point where wait has succeeded. 
#define ANNOTATE_CONDVAR_LOCK_WAIT(cv, lock) \
            AnnotateCondVarWait(__FILE__, __LINE__, cv, lock)

// Variant of ANNOTATE_CONDVAR_LOCK_WAIT, but w/o lock.
#define ANNOTATE_CONDVAR_WAIT(cv) \
            AnnotateCondVarWait(__FILE__, __LINE__, cv, NULL)

// Insert right before the signal. 
#define ANNOTATE_CONDVAR_SIGNAL(cv) \
            AnnotateCondVarSignal(__FILE__, __LINE__, cv)
// Same as ANNOTATE_CONDVAR_SIGNAL.
#define ANNOTATE_CONDVAR_SIGNAL_ALL(cv) \
            AnnotateCondVarSignalAll(__FILE__, __LINE__, cv)

// Aliases for ANNOTATE_CONDVAR_SIGNAL / ANNOTATE_CONDVAR_WAIT
#define ANNOTATE_HAPPENS_BEFORE ANNOTATE_CONDVAR_SIGNAL
#define ANNOTATE_HAPPENS_AFTER  ANNOTATE_CONDVAR_WAIT

// Insert into the PCQ constructor. 
#define ANNOTATE_PCQ_CREATE(pcq) \
            AnnotatePCQCreate(__FILE__, __LINE__, pcq)

// Insert into the PCQ destructor. 
#define ANNOTATE_PCQ_DESTROY(pcq) \
            AnnotatePCQDestroy(__FILE__, __LINE__, pcq)

// Insert right before putting element into the queue (in Put()). 
#define ANNOTATE_PCQ_PUT(pcq) \
            AnnotatePCQPut(__FILE__, __LINE__, pcq)

// Insert right after getting element from the queue (in Get()). 
#define ANNOTATE_PCQ_GET(pcq) \
            AnnotatePCQGet(__FILE__, __LINE__, pcq)

// Insert this right after the memory is allocated by 
// a non-standard memory allocator,
// e.g. after it is extracted from a free-list.
#define ANNOTATE_NEW_MEMORY(mem, size) \
            AnnotateNewMemory(__FILE__, __LINE__, mem, size)

// Report that the object of size 'size' stored in 'object_pointer'
// has been created and is about to be published safely. 
// The race checker will create a happens-before arc between the call 
// ANNOTATE_PUBLISH_MEMORY_RANGE(object,size) and subsequent 
// reads from a memory in the range [object, object+size).
#define ANNOTATE_PUBLISH_MEMORY_RANGE(object_pointer, size) \
            AnnotatePublishMemoryRange(__FILE__, __LINE__, object_pointer, size);

// A variation of ANNOTATE_PUBLISH_MEMORY_RANGE with the size 
// computed by sizeof(*object_pointer).
#define ANNOTATE_PUBLISH_OBJECT(object_pointer) \
            ANNOTATE_PUBLISH_MEMORY_RANGE(object_pointer, sizeof(*object_pointer))

// Insert at the beginning of a unit test. 
#define ANNOTATE_EXPECT_RACE(mem, description) \
            AnnotateExpectRace(__FILE__, __LINE__, mem, description)

// Marks that we have a benign race on 'mem'.
// Insert at the point where 'mem' exists, preferably close to the point
// where the race happens.
#define ANNOTATE_BENIGN_RACE(arg, description) \
            AnnotateBenignRace(__FILE__, __LINE__, arg, description)

// Report that the mutex 'mu' should be treated as in pure happens-before detector.
#define ANNOTATE_MUTEX_IS_USED_AS_CONDVAR(mu) \
            AnnotateMutexIsUsedAsCondVar(__FILE__, __LINE__, ((char*)mu + sizeof(pthread_cond_t)))


// Report that we want to trace all memory accesses to this memory location,
#define ANNOTATE_TRACE_MEMORY(arg) \
            AnnotateTraceMemory(__FILE__, __LINE__, arg)

// Report the current thread name to a race detector.
#define ANNOTATE_THREAD_NAME(name) \
            AnnotateThreadName(__FILE__, __LINE__, name)


// A no-op. Insert where you like for testing the interceptors. 
#define ANNOTATE_NO_OP(arg) \
            AnnotateNoOp(__FILE__, __LINE__, arg)

// Instruct the race detector to ignore all reads in the section 
// defined by ANNOTATE_IGNORE_READS_BEGIN/ANNOTATE_IGNORE_READS_END
#define ANNOTATE_IGNORE_READS_BEGIN() \
            AnnotateIgnoreReadsBegin(__FILE__, __LINE__, NULL /*reserved*/)
#define ANNOTATE_IGNORE_READS_END() \
            AnnotateIgnoreReadsEnd(__FILE__, __LINE__, NULL /*reserved*/)

// Same for writes.
#define ANNOTATE_IGNORE_WRITES_BEGIN() \
            AnnotateIgnoreWritesBegin(__FILE__, __LINE__, NULL /*reserved*/)
#define ANNOTATE_IGNORE_WRITES_END() \
            AnnotateIgnoreWritesEnd(__FILE__, __LINE__, NULL /*reserved*/)

template <class T>
inline T ANNOTATE_UNPROTECTED_READ(const volatile T &x) {
  ANNOTATE_IGNORE_READS_BEGIN();
  T res = x;
  ANNOTATE_IGNORE_READS_END();
  return res;
}

#define ANNOTATE_PRINT_MEMORY_USAGE(flags) \
            AnnotatePrintMemoryUsage(__FILE__, __LINE__, flags)
#define ANNOTATE_PRINT_STATS() \
            AnnotatePrintStats(__FILE__, __LINE__, NULL /*reserved*/)
#define ANNOTATE_RESET_STATS() \
            AnnotateResetStats(__FILE__, __LINE__, NULL /*reserved*/)

#endif  // DYNAMIC_ANNOTATIONS_H__/
