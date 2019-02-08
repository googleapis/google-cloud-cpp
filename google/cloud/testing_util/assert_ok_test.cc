// Copyright 2019 Google LLC
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

#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/status.h"
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

TEST(AssertOkTest, AssertionOk) {
  ::google::cloud::Status status;
  ASSERT_OK(status);
}

TEST(AssertOkTest, AssertionOkDescription) {
  ::google::cloud::Status status;
  ASSERT_OK(status) << "OK is not OK?";
}

TEST(AssertOkTest, AssertionFailed) {
  EXPECT_FATAL_FAILURE(
      {
        ::google::cloud::Status status(::google::cloud::StatusCode::kInternal,
                                       "oh no!");
        ASSERT_OK(status);
      },
      "Status of \"status\" is expected to be OK, but evaluates to \"oh no!\" "
      "(code INTERNAL)");
}

TEST(AssertOkTest, AssertionFailedDescription) {
  EXPECT_FATAL_FAILURE(
      {
        ::google::cloud::Status status(::google::cloud::StatusCode::kInternal,
                                       "oh no!");
        ASSERT_OK(status) << "my precious assertion failed";
      },
      "Status of \"status\" is expected to be OK, but evaluates to \"oh no!\" "
      "(code INTERNAL)\nmy precious assertion failed");
}

TEST(ExpectOkTest, ExpectOk) {
  ::google::cloud::Status status;
  EXPECT_OK(status);
}

TEST(ExpectOkTest, ExpectionOkDescription) {
  ::google::cloud::Status status;
  EXPECT_OK(status) << "OK is not OK?";
}

TEST(ExpectOkTest, ExpectionFailed) {
  EXPECT_NONFATAL_FAILURE(
      {
        ::google::cloud::Status status(::google::cloud::StatusCode::kInternal,
                                       "oh no!");
        EXPECT_OK(status);
      },
      "Status of \"status\" is expected to be OK, but evaluates to \"oh no!\" "
      "(code INTERNAL)");
}

TEST(ExpectOkTest, ExpectionFailedDescription) {
  EXPECT_NONFATAL_FAILURE(
      {
        ::google::cloud::Status status(::google::cloud::StatusCode::kInternal,
                                       "oh no!");
        EXPECT_OK(status) << "my precious assertion failed";
      },
      "Status of \"status\" is expected to be OK, but evaluates to \"oh no!\" "
      "(code INTERNAL)\nmy precious assertion failed");
}
