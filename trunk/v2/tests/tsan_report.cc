//===-- tsan_report.cc ------------------------------------------*- C++ -*-===//
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
//===----------------------------------------------------------------------===//
#include "tsan_interface.h"
#include "tsan_test_util.h"
#include "tsan_defs.h"
#include "tsan_report.h"
#include "gtest/gtest.h"
#include <stddef.h>
#include <stdint.h>

TEST(ThreadSanitizer, ReportBasic) {
  ScopedThread t1, t2;
  MemLoc l;
  t1.Read2(l);
  const ReportDesc *rep = t2.Write4(l, true);
  CHECK_NE(rep, NULL);
  CHECK_EQ(rep->typ, __tsan::ReportTypeRace);
  CHECK_EQ(rep->nmop, 2);
  CHECK_NE(rep->mop[0].tid, 0);
  CHECK_EQ(rep->mop[0].addr, (uintptr_t)l.loc());
  CHECK_EQ(rep->mop[0].size, 4);
  CHECK_EQ(rep->mop[0].write, true);
  CHECK_EQ(rep->mop[0].nmutex, 0);
  CHECK_EQ(rep->mop[0].stack.cnt, 1);
  CHECK_NE(rep->mop[0].stack.entry[0].pc, 0);
  CHECK_NE(rep->mop[0].stack.entry[0].func, NULL);
  CHECK_NE(rep->mop[0].stack.entry[0].file, NULL);
  CHECK_NE(rep->mop[0].stack.entry[0].line, 0);
  CHECK_NE(rep->mop[1].tid, 0);
  CHECK_NE(rep->mop[1].tid, rep->mop[0].tid);
  CHECK_EQ(rep->mop[1].addr, (uintptr_t)l.loc());
  CHECK_EQ(rep->mop[1].size, 2);
  CHECK_EQ(rep->mop[1].write, false);
  CHECK_EQ(rep->mop[1].nmutex, 0);
  CHECK_EQ(rep->mop[1].stack.cnt, 1);
  CHECK_NE(rep->mop[1].stack.entry[0].pc, 0);
  CHECK_NE(rep->mop[1].stack.entry[0].func, NULL);
  CHECK_NE(rep->mop[1].stack.entry[0].file, NULL);
  CHECK_NE(rep->mop[1].stack.entry[0].line, 0);
  CHECK_EQ(rep->loc, NULL);
  CHECK_EQ(rep->nthread, 0);
  CHECK_EQ(rep->nmutex, 0);
}

static uintptr_t NOINLINE get_pc() {
  return (uintptr_t)__builtin_return_address(0);
}

static void foo() {}; static int foo_line = __LINE__;
static void bar() {}; static int bar_line = __LINE__;

TEST(ThreadSanitizer, ReportStack) {
  ScopedThread t1;
  MemLoc l;
  const char *func = __FUNCTION__;
  const char *file = __FILE__;
  uintptr_t pc = get_pc();
  __tsan_write1(l.loc()); int line = __LINE__;
  t1.Call(&foo);
  t1.Call(&bar);
  const ReportDesc *rep = t1.Write1(l, true);
  CHECK_EQ(rep->typ, __tsan::ReportTypeRace);
  CHECK_EQ(rep->nmop, 2);
  CHECK_NE(rep->mop[0].tid, 0);
  CHECK_EQ(rep->mop[0].addr, (uintptr_t)l.loc());
  CHECK_EQ(rep->mop[0].size, 1);
  CHECK_EQ(rep->mop[0].write, true);
  CHECK_EQ(rep->mop[0].stack.cnt, 3);
  CHECK_EQ(rep->mop[0].stack.entry[1].pc, (uintptr_t)(void*)&bar);
  CHECK(strstr(rep->mop[0].stack.entry[1].func, "bar"));
  CHECK(strstr(rep->mop[0].stack.entry[1].file, file));
  CHECK_EQ(rep->mop[0].stack.entry[1].line, bar_line);
  CHECK_EQ(rep->mop[0].stack.entry[2].pc, (uintptr_t)(void*)&foo);
  CHECK(strstr(rep->mop[0].stack.entry[2].func, "foo"));
  CHECK(strstr(rep->mop[0].stack.entry[2].file, file));
  CHECK_EQ(rep->mop[0].stack.entry[2].line, foo_line);
  CHECK_EQ(rep->mop[1].tid, 0);
  CHECK_EQ(rep->mop[1].addr, (uintptr_t)l.loc());
  CHECK_EQ(rep->mop[1].size, 1);
  CHECK_EQ(rep->mop[1].write, true);
  CHECK_EQ(rep->mop[1].stack.cnt, 1);
  CHECK_GT(rep->mop[1].stack.entry[0].pc, pc - 64);
  CHECK_LT(rep->mop[1].stack.entry[0].pc, pc + 64);
  CHECK(strstr(rep->mop[1].stack.entry[0].func, func));
  CHECK(strstr(rep->mop[1].stack.entry[0].file, file));
  CHECK_EQ(rep->mop[1].stack.entry[0].line, line);
  (void)foo_line;
  (void)bar_line;
}