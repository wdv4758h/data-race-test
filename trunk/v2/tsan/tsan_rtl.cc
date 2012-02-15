//===-- tsan_rtl.cc ---------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
// Main file (entry points) for the TSan run-time.
//===----------------------------------------------------------------------===//

#include "tsan_platform.h"
#include "tsan_rtl.h"
#include "tsan_interface.h"
#include "tsan_atomic.h"
#include "tsan_placement_new.h"
#include "tsan_suppressions.h"
#include "tsan_symbolize.h"
#include "tsan_sync.h"
#include "tsan_report.h"

namespace __tsan {

__thread char cur_thread_placeholder[sizeof(ThreadState)] ALIGN(64);
static char ctx_placeholder[sizeof(Context)] ALIGN(64);
static ReportDesc g_report;

union Shadow {
  struct {
    u64 tid   : kTidBits;
    u64 epoch : kClkBits;
    u64 addr0 : 3;
    u64 addr1 : 3;
    u64 write : 1;
  };
  u64 raw;
};

u64 min(u64 a, u64 b) {
  return a < b ? a : b;
}

u64 max(u64 a, u64 b) {
  return a > b ? a : b;
}

Context *ctx;

void CheckFailed(const char *file, int line, const char *cond) {
  Report("FATAL: ThreadSanitizer CHECK failed: %s:%d \"%s\"\n",
         file, line, cond);
  Die();
}

void TraceSwitch(ThreadState *thr) {
  Lock l(&thr->trace.mtx);
  int trace = (thr->fast.epoch / (kTraceSize / kTraceParts)) % kTraceParts;
  thr->trace.headers[trace].epoch0 = thr->fast.epoch;
}

Context::Context()
  : clockslab(SyncClock::kChunkSize)
  , syncslab(sizeof(SyncVar)) {
}

ThreadState::ThreadState(Context *ctx)
  : clockslab(&ctx->clockslab)
  , syncslab(&ctx->syncslab) {
}

ThreadContext::ThreadContext(int tid)
  : tid(tid)
  , thr()
  , status(ThreadStatusInvalid)
  , uid()
  , detached()
  , reuse_count()
  , epoch0()
  , dead_next() {
}

void Initialize(ThreadState *thr) {
  // Thread safe because done before all threads exist.
  if (ctx)
    return;
  DPrintf("tsan::Initialize\n");
  ctx = new(ctx_placeholder) Context;
  InitializeShadowMemory();
  ctx->dead_list_size = 0;
  ctx->dead_list_head = 0;
  ctx->dead_list_tail = 0;
  InitializeInterceptors();
  InitializeSuppressions();

  // Initialize thread 0.
  ctx->thread_seq = 0;
  int tid = ThreadCreate(thr, 0, true);
  CHECK_EQ(tid, 0);
  ThreadStart(thr, tid);
}

template<typename T>
static T* alloc(ReportDesc *rep, int n, int *pos) {
  T* p = (T*)(rep->alloc + *pos);
  *pos += n * sizeof(T);
  CHECK(*pos <= (int)sizeof(rep->alloc));
  return p;
}

static int RestoreStack(int tid, u64 epoch, uptr *stack, int n) {
  Lock l0(&ctx->thread_mtx);
  ThreadContext *tctx = ctx->threads[tid];
  if (tctx == 0)
    return 0;
  Trace* trace = 0;
  if (tctx->status == ThreadStatusRunning) {
    CHECK(tctx->thr);
    trace = &tctx->thr->trace;
  } else if (tctx->status == ThreadStatusFinished
      || tctx->status == ThreadStatusDead) {
    trace = &tctx->dead_info.trace;
  } else {
    return 0;
  }
  Lock l(&trace->mtx);
  TraceHeader* hdr = &trace->headers[
      (epoch / (kTraceSize / kTraceParts)) % kTraceParts];
  if (epoch < hdr->epoch0)
    return 0;
  epoch %= (kTraceSize / kTraceParts);
  u64 pos = 0;
  for (u64 i = 0; i <= epoch; i++) {
    Event ev = trace->events[i];
    EventType typ = (EventType)(ev >> 61);
    uptr pc = (uptr)(ev & 0xffffffffffffull);
    if (typ == EventTypeMop) {
      stack[pos] = pc;
    } else if (typ == EventTypeFuncEnter) {
      stack[pos++] = pc;
    } else if (typ == EventTypeFuncExit) {
      pos--;
    }
  }
  pos++;
  for (u64 i = 0; i <= pos / 2; i++) {
    uptr pc = stack[i];
    stack[i] = stack[pos - i - 1];
    stack[pos - i - 1] = pc;
  }
  return pos;
}

static void NOINLINE ReportRace(ThreadState *thr, uptr addr,
                                Shadow s0, Shadow s1) {
  Lock l(&ctx->report_mtx);
  addr &= ~7;
  int alloc_pos = 0;
  ReportDesc &rep = g_report;
  rep.typ = ReportTypeRace;
  rep.nmop = 2;
  rep.mop = alloc<ReportMop>(&rep, rep.nmop, &alloc_pos);
  for (int i = 0; i < rep.nmop; i++) {
    ReportMop *mop = &rep.mop[i];
    Shadow *s = (i ? &s1 : &s0);
    mop->tid = s->tid;
    mop->addr = addr + s->addr0;
    mop->size = s->addr1 - s->addr0 + 1;
    mop->write = s->write;
    mop->nmutex = 0;
    uptr stack[64];
    mop->stack.cnt = RestoreStack(s->tid, s->epoch, stack,
                                  sizeof(stack)/sizeof(stack[0]));
    if (mop->stack.cnt != 0) {
      mop->stack.entry = alloc<ReportStackEntry>(&rep, mop->stack.cnt,
                                                   &alloc_pos);
      for (int i = 0; i < mop->stack.cnt; i++) {
        ReportStackEntry *ent = &mop->stack.entry[i];
        ent->pc = stack[i];
        ent->pc = stack[i];
        ent->func = alloc<char>(&rep, 1024, &alloc_pos);
        ent->func[0] = 0;
        ent->file = alloc<char>(&rep, 1024, &alloc_pos);
        ent->file[0] = 0;
        ent->line = 0;
        SymbolizeCode(ent->pc, ent->func, 1024, ent->file, 1024, &ent->line);
      }
    }
  }
  rep.loc = 0;
  rep.nthread = 0;
  rep.nmutex = 0;
  bool suppressed = IsSuppressed(ReportTypeRace, &rep.mop[0].stack);
  suppressed = OnReport(&rep, suppressed);
  if (suppressed)
    return;
  PrintReport(&rep);
}

ALWAYS_INLINE
static Shadow LoadShadow(u64 *p) {
  Shadow s;
  s.raw = atomic_load((atomic_uint64_t*)p, memory_order_relaxed);
  return s;
}

ALWAYS_INLINE
static void StoreShadow(u64 *p, u64 raw) {
  atomic_store((atomic_uint64_t*)p, raw, memory_order_relaxed);
}

ALWAYS_INLINE
static bool MemoryAccess1(ThreadState *thr, ThreadState::Fast fast_state,
                          u64 synch_epoch, Shadow s0, u64 *sp, bool is_write,
                          bool &replaced, Shadow &racy_access) {
  Shadow s = LoadShadow(sp);
  if (s.raw == 0) {
    if (replaced == false) {
      StoreShadow(sp, s0.raw);
      replaced = true;
    }
    return false;
  }
  // is the memory access equal to the previous?
  if (s0.addr0 == s.addr0 && s0.addr1 == s.addr1) {
    // same thread?
    if (s.tid == fast_state.tid) {
      if (s.epoch >= synch_epoch) {
        if (s.write || !is_write) {
          // found a slot that holds effectively the same info
          // (that is, same tid, same sync epoch and same size)
          return true;
        } else {
          StoreShadow(sp, replaced ? 0ull : s0.raw);
          replaced = true;
          return false;
        }
      } else {
        if (!s.write || is_write) {
          StoreShadow(sp, replaced ? 0ull : s0.raw);
          replaced = true;
          return false;
        } else {
          return false;
        }
      }
    } else {
      // happens before?
      if (thr->clock.get(s.tid) >= s.epoch) {
        StoreShadow(sp, replaced ? 0ull : s0.raw);
        replaced = true;
        return false;
      } else if (!s.write && !is_write) {
        return false;
      } else {
        racy_access = s;
        return false;
      }
    }
  // do the memory access intersect?
  } else if (min(s0.addr1, s.addr1) >= max(s0.addr0, s.addr0)) {
    if (s.tid == fast_state.tid)
      return false;
    // happens before?
    if (thr->clock.get(s.tid) >= s.epoch) {
      return false;
    } else if (!s.write && !is_write) {
      return false;
    } else {
      racy_access = s;
      return false;
    }
  }
  // the accesses do not intersect
  return false;
}

ALWAYS_INLINE
void MemoryAccess(ThreadState *thr, uptr pc, uptr addr,
                  int size, bool is_write) {
  StatInc(thr, StatMop);
  u64 *shadow_mem = (u64*)MemToShadow(addr);
  DPrintf("#%d: tsan::OnMemoryAccess: @%p %p size=%d"
          " is_write=%d shadow_mem=%p\n",
          (int)thr->fast.tid, (void*)pc, (void*)addr,
          (int)size, is_write, shadow_mem);
  DCHECK(IsAppMem(addr));
  DCHECK(IsShadowMem((uptr)shadow_mem));

  ThreadState::Fast fast_state;
  fast_state.raw = thr->fast.raw;  // Copy.
  fast_state.epoch++;
  thr->fast.raw = fast_state.raw;
  TraceAddEvent(thr, fast_state.epoch, EventTypeMop, pc);

  // descriptor of the memory access
  Shadow s0 = { {fast_state.tid, fast_state.epoch,
               addr&7, min((addr&7)+size-1, 7), is_write} };  // NOLINT
  // Is the descriptor already stored somewhere?
  bool replaced = false;
  // Racy memory access. Zero if none.
  Shadow racy_access;
  racy_access.raw = 0;

  // scan all the shadow values and dispatch to 4 categories:
  // same, replace, candidate and race (see comments below).
  // we consider only 3 cases regarding access sizes:
  // equal, intersect and not intersect. initially I considered
  // larger and smaller as well, it allowed to replace some
  // 'candidates' with 'same' or 'replace', but I think
  // it's just not worth it (performance- and complexity-wise).
  const u64 synch_epoch = thr->fast_synch_epoch;

  // The idea behind the offset is as follows.
  // Consider that we have 8 bool's contained within a single 8-byte block
  // (mapped to a single shadow "cell"). Now consider that we write to the bools
  // from a single thread (which we consider the common case).
  // W/o offsetting each access will have to scan 4 shadow values at average
  // to find the corresponding shadow value for the bool.
  // With offsetting we start scanning shadow with the offset so that
  // each access hits necessary shadow straight off (at least in an expected
  // optimistic case).
  // This logic works seamlessly for any layout of user data. For example,
  // if user data is {int, short, char, char}, then accesses to the int are
  // offsetted to 0, short - 4, 1st char - 6, 2nd char - 7. Hopefully, accesses
  // from a single thread won't need to scan all 8 shadow values.
  int off = 0;
  if (size == 1)
    off = addr & 7;
  else if (size == 2)
    off = addr & 6;
  else if (size == 4)
    off = addr & 4;

  for (int i = 0; i < kShadowCnt; i++) {
    u64 *sp = &shadow_mem[(i + off) % kShadowCnt];
    if (MemoryAccess1(thr, fast_state, synch_epoch, s0, sp, is_write,
                      replaced, racy_access))
      return;
  }

  // find some races?
  if (UNLIKELY(racy_access.raw != 0))
    ReportRace(thr, addr, s0, racy_access);
  // we did not find any races and had already stored
  // the current access info, so we are done
  if (LIKELY(replaced))
    return;
  // choose a random candidate slot and replace it
  unsigned i = fast_state.epoch % kShadowCnt;
  StoreShadow(shadow_mem+i, s0.raw);
}

void FuncEntry(ThreadState *thr, uptr pc) {
  StatInc(thr, StatFuncEnter);
  DPrintf("#%d: tsan::FuncEntry %p\n", (int)thr->fast.tid, (void*)pc);
  thr->fast.epoch++;
  TraceAddEvent(thr, thr->fast.epoch, EventTypeFuncEnter, pc);
}

void FuncExit(ThreadState *thr) {
  StatInc(thr, StatFuncExit);
  DPrintf("#%d: tsan::FuncExit\n", (int)thr->fast.tid);
  thr->fast.epoch++;
  TraceAddEvent(thr, thr->fast.epoch, EventTypeFuncExit, 0);
}

}  // namespace __tsan

// Must be included in this file to make sure everything is inlined.
#include "tsan_interface_inl.h"
