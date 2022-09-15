// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/terminate_handler.h"
#include <gtest/gtest.h>
#include <iostream>
#include <string>

using ::google::cloud::GetTerminateHandler;
using ::google::cloud::SetTerminateHandler;
using ::google::cloud::Terminate;
using ::google::cloud::TerminateHandler;

namespace {

constexpr char kHandlerMsg[] = "Custom handler invoked. Extra description: ";

void CustomHandler(char const* msg) {
  std::cerr << kHandlerMsg << msg << "\n";
  abort();
}

void CustomHandlerOld(char const*) { abort(); }

}  // namespace

TEST(TerminateHandler, UnsetTerminates) {
  EXPECT_DEATH_IF_SUPPORTED(Terminate("Test"),
                            "Aborting because exceptions are disabled: Test");
}

TEST(TerminateHandler, SettingGettingWorks) {
  auto orig = SetTerminateHandler(&CustomHandler);
  TerminateHandler set_handler = GetTerminateHandler();
  ASSERT_TRUE(CustomHandler == *set_handler.target<void (*)(char const*)>())
      << "The handler objects should be equal.";
  SetTerminateHandler(orig);
}

TEST(TerminateHandler, OldHandlerIsReturned) {
  auto orig = SetTerminateHandler(&CustomHandlerOld);
  TerminateHandler old_handler = SetTerminateHandler(CustomHandler);
  ASSERT_TRUE(CustomHandlerOld == *old_handler.target<void (*)(char const*)>())
      << "The handler objects should be equal.";
  SetTerminateHandler(orig);
}

TEST(TerminateHandler, TerminateTerminates) {
  auto orig = SetTerminateHandler(&CustomHandler);
  const std::string expected = std::string(kHandlerMsg) + "details";
  EXPECT_DEATH_IF_SUPPORTED(Terminate("details"), expected);
  SetTerminateHandler(orig);
}

TEST(TerminateHandler, NoAbortAborts) {
  auto orig = SetTerminateHandler([](char const*) {});
  const std::string expected =
      "Aborting because the installed terminate "
      "handler returned. Error details: details";
  EXPECT_DEATH_IF_SUPPORTED(Terminate("details"), expected);
  SetTerminateHandler(orig);
}
