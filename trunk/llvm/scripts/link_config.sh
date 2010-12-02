#!/bin/bash
#
# Configure the LDFLAGS for linker scripts.

wrap () {
  LDFLAGS+="-Wl,--wrap,$1 "
}

undefined () {
  LDFLAGS+="-Wl,--undefined,$1 "
}

LDFLAGS="-lpthread "

wrap __cxa_guard_acquire
wrap __cxa_guard_release

wrap pthread_create
wrap pthread_join

wrap pthread_mutex_init
wrap pthread_mutex_destroy
wrap pthread_mutex_lock
wrap pthread_mutex_unlock
wrap pthread_mutex_trylock

wrap pthread_rwlock_init
wrap pthread_rwlock_destroy
wrap pthread_rwlock_tryrdlock
wrap pthread_rwlock_rdlock
wrap pthread_rwlock_trywrlock
wrap pthread_rwlock_wrlock
wrap pthread_rwlock_unlock

wrap pthread_spin_init
wrap pthread_spin_destroy
wrap pthread_spin_lock
wrap pthread_spin_trylock
wrap pthread_spin_unlock

wrap pthread_cond_signal
wrap pthread_cond_wait
wrap pthread_cond_timedwait

wrap pthread_barrier_init
wrap pthread_barrier_wait

wrap sem_open
wrap sem_post
wrap sem_wait
wrap sem_trywait

wrap atexit
wrap exit

wrap strlen
wrap strcmp

wrap mmap
wrap munmap
wrap calloc
wrap malloc
wrap free
wrap realloc

wrap read
wrap write

wrap pthread_once

wrap sigaction

# operator new(unsigned int)
wrap _Znwj
wrap _Znwm
# operator delete(void*)
wrap _ZdlPv

wrap memcpy
wrap memmove

# TODO(glider): without defining this annotation bigtest is missing
# LLVMAnnotateCondVarSignal(). Strange enough, to be investigated.
# Also "LLVM" is a prefix that may change in the future.
undefined LLVMAnnotateCondVarSignal
