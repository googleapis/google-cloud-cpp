// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>

/// @test a trivial test to keep the compiler happy when all tests are disabled.
TEST(ForceSanitizerFailuresTest, Trivial) {}

#ifdef BIGTABLE_CLIENT_FORCE_STATIC_ANALYZER_ERRORS
TEST(ForceScanBuildDiagnostic, Test) {
  int r = std::rand();
  if (r != 0) {
    int x = std::numeric_limits<int>::max() / r;
    EXPECT_LE(0, x);
  } else {
    int x = std::numeric_limits<int>::min() / r;
    EXPECT_GE(0, x);
  }
}
#endif  // BIGTABLE_CLIENT_FORCE_STATIC_ANALYZER_ERRORS

// These tests are only used when testing the CI scripts, we want to keep them
// as documentation and a quick way to exercise the tests.  It might be
// interesting to figure out a way to always enable these tests.
#ifdef BIGTABLE_CLIENT_FORCE_SANITIZER_ERRORS
namespace {
/// Return a value not-known at compile time.
long ChangeValueAtRuntime(int x) {
  // avoid undefined behavior (overflow and underflow), but make the result
  // depend on both the input and something not-known at compile-time.
  if (x >= 0) {
    return x - std::rand();
  }
  return x + std::rand();
}
}  // anonymous namespace

/// @test Force an error detected by the AddressSanitizer.
TEST(ForceSanitizerFailuresTest, AddressSanitizer) {
  int* array = new int[1000];
  array[100] = 42;
  delete[] array;
  // We do not want the EXPECT_*() to fail, test for something trivially true.
  EXPECT_GE(std::numeric_limits<int>::max(), ChangeValueAtRuntime(array[100]));
}

/// @test Force an error detected by the LeaksSanitizer.
TEST(ForceSanitizerFailuresTest, LeaksSanitizer) {
  int* array = new int[1000];
  array[100] = 42;
  EXPECT_EQ(42, array[100]);
  array = nullptr;
}

/// @test Force an error detected by the MemorySanitizer.
TEST(ForceSanitizerFailuresTest, MemorySanitizer) {
  int* array = new int[1000];
  array[100] = 42;
  // We do not want the EXPECT_*() to fail, test for something trivially true.
  EXPECT_GE(std::numeric_limits<int>::max(), ChangeValueAtRuntime(array[10]));
  delete[] array;
}

/// @test Force an error detected by the UndefinedBehaviorSanitizer.
TEST(ForceSanitizerFailuresTest, UndefinedBehaviorSanitizer) {
  int overflow = std::numeric_limits<int>::max();
  // Use rand() to avoid a clever compiler detecting a problem at compile-time.
  overflow += std::rand();
  overflow += std::rand();
  EXPECT_NE(0, overflow);
}

#endif  // BIGTABLE_CLIENT_FORCE_SANITIZER_ERRORS
