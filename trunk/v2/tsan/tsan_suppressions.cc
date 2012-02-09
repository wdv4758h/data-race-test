//===-- tsan_suppressions.cc ------------------------------------*- C++ -*-===//
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
#include "tsan_suppressions.h"

namespace __tsan {

void InitializeSuppressions() {
}

bool IsSuppressed(ReportType typ, const ReportStack *stack) {
  (void)typ;
  (void)stack;
  return false;
}

}  // namespace __tsan
