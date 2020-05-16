// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/crash_handler.h"
#include <gtest/gtest.h>

TEST(CrashHandler, InstallCrashHandler) {
  // Abseil has already tested that the failure signal handler and symbolizer
  // works. It's unclear how exactly to test that this function correctly
  // "installs" them in the program. But this test calls the function for code
  // coverage and it gives us a convenient place to add more tests if we
  // discover better ways to test this.
  auto const argv0 = "fake_argv0_but_thats_ok_for_a_test";
  google::cloud::testing_util::InstallCrashHandler(argv0);
}
