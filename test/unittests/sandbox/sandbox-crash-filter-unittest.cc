// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "src/sandbox/sandbox-zero-segment.h"
#include "src/sandbox/sandbox.h"
#include "src/sandbox/testing.h"
#include "test/unittests/test-utils.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

#ifdef V8_OS_LINUX

using SandboxCrashFilterTest = TestWithIsolate;

// Verify that the crash filter correctly identifies accesses within the first
// 4GB of virtual address space (the zero segment base) as harmless.
TEST_F(SandboxCrashFilterTest, SandboxCrashFilterZeroSegmentBaseAccess) {
  ASSERT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        volatile char* p = reinterpret_cast<volatile char*>(0x10000);
        char c = *p;
        (void)c;
      },
      testing::ExitedWithCode(0),
      "Caught harmless memory access violation \\(first 4GB of virtual address "
      "space\\)");
}

// Verify that the crash filter correctly identifies accesses within the zero
// segment padding (past 4GB) as harmless. HeapObject confusion in JS and Wasm
// can lead to any access within kZeroSegmentPadding.
TEST_F(SandboxCrashFilterTest, SandboxCrashFilterZeroSegmentPaddingAccess) {
  ASSERT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        volatile char* p = reinterpret_cast<volatile char*>(
            kZeroSegmentBaseSize + kZeroSegmentPadding - 1);
        char c = *p;
        (void)c;
      },
      testing::ExitedWithCode(0),
      "Caught harmless memory access violation \\(first 4GB of virtual address "
      "space\\)");
}

#endif  // V8_OS_LINUX

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX
