// Copyright 2018 Google LLC
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

#include "google/cloud/terminate_handler.h"
#include <gtest/gtest.h>
#include <iostream>

using namespace google::cloud;

namespace {
const std::string handler_msg = "Custom handler invoked. Extra description: ";

void CustomHandler(const char* msg) {
  std::cerr << handler_msg << msg << "\n";
  abort();
}

void CustomHandlerOld(const char*) { abort(); }
}  // namespace

TEST(TerminateHandler, UnsetTerminates) {
  GetTerminateHandler();
  EXPECT_DEATH_IF_SUPPORTED(Terminate("Test"),
                            "Aborting because exceptions are disabled: Test");
}

TEST(TerminateHandler, SettingGettingWorks) {
  SetTerminateHandler(&CustomHandler);
  TerminateHandler set_handler = GetTerminateHandler();
  ASSERT_TRUE(CustomHandler == *set_handler.target<void (*)(const char*)>())
      << "The handler objects should be equal.";
}

TEST(TerminateHandler, OldHandlerIsReturned) {
  SetTerminateHandler(&CustomHandlerOld);
  TerminateHandler old_handler = SetTerminateHandler(CustomHandler);
  ASSERT_TRUE(CustomHandlerOld == *old_handler.target<void (*)(const char*)>())
      << "The handler objects should be equal.";
}

TEST(TerminateHandler, TerminateTerminates) {
  SetTerminateHandler(&CustomHandler);
  EXPECT_DEATH_IF_SUPPORTED(Terminate("details"), handler_msg + "details");
}

TEST(TerminateHandler, NoAbortAborts) {
  SetTerminateHandler([](const char*) {});
  const std::string expected =
      "Aborting because the installed terminate "
      "handler returned. Error details: details";

  EXPECT_DEATH_IF_SUPPORTED(Terminate("details"), expected);
}
