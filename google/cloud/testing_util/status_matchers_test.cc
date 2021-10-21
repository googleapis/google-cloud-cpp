// Copyright 2020 Google LLC
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

#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

using ::testing::_;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

TEST(AssertOkTest, AssertionOk) {
  Status status;
  ASSERT_STATUS_OK(status);
}

TEST(AssertOkTest, AssertionOkStatusOr) {
  StatusOr<int> status_or(42);
  ASSERT_STATUS_OK(status_or);
}

TEST(AssertOkTest, AssertionOkDescription) {
  Status status;
  ASSERT_STATUS_OK(status) << "OK is not OK?";
}

TEST(AssertOkTest, AssertionOkDescriptionStatusOr) {
  StatusOr<int> status_or(42);
  ASSERT_STATUS_OK(status_or) << "OK is not OK?";
}

TEST(AssertOkTest, AssertionFailed) {
  EXPECT_FATAL_FAILURE(
      {
        Status status(StatusCode::kInternal, "oh no!");
        ASSERT_STATUS_OK(status);
      },
      "\n  Actual: oh no! [INTERNAL]");
}

TEST(AssertOkTest, AssertionFailedStatusOr) {
  EXPECT_FATAL_FAILURE(
      {
        StatusOr<int> status_or(Status(StatusCode::kInternal, "oh no!"));
        ASSERT_STATUS_OK(status_or);
      },
      ", whose status is oh no! [INTERNAL]");
}

TEST(AssertOkTest, AssertionFailedDescription) {
  EXPECT_FATAL_FAILURE(
      {
        Status status(StatusCode::kInternal, "oh no!");
        ASSERT_STATUS_OK(status) << "my assertion failed";
      },
      "\nmy assertion failed");
}

TEST(AssertOkTest, AssertionFailedDescriptionStatusOr) {
  EXPECT_FATAL_FAILURE(
      {
        StatusOr<int> status_or(Status(StatusCode::kInternal, "oh no!"));
        ASSERT_STATUS_OK(status_or) << "my assertion failed";
      },
      "\nmy assertion failed");
}

TEST(ExpectOkTest, ExpectOk) {
  Status status;
  EXPECT_STATUS_OK(status);
}

TEST(ExpectOkTest, ExpectOkStatusOr) {
  StatusOr<int> status_or(42);
  EXPECT_STATUS_OK(status_or);
}

TEST(ExpectOkTest, ExpectationOkDescription) {
  Status status;
  EXPECT_STATUS_OK(status) << "OK is not OK?";
}

TEST(ExpectOkTest, ExpectationOkDescriptionStatusOr) {
  StatusOr<int> status_or(42);
  EXPECT_STATUS_OK(status_or) << "OK is not OK?";
}

TEST(ExpectOkTest, ExpectationFailed) {
  EXPECT_NONFATAL_FAILURE(
      {
        Status status(StatusCode::kInternal, "oh no!");
        EXPECT_STATUS_OK(status);
      },
      "\n  Actual: oh no! [INTERNAL]");
}

TEST(ExpectOkTest, ExpectationFailedStatusOr) {
  EXPECT_NONFATAL_FAILURE(
      {
        StatusOr<int> status_or(Status(StatusCode::kInternal, "oh no!"));
        EXPECT_STATUS_OK(status_or);
      },
      ", whose status is oh no! [INTERNAL]");
}

TEST(ExpectOkTest, ExpectationFailedDescription) {
  EXPECT_NONFATAL_FAILURE(
      {
        Status status(StatusCode::kInternal, "oh no!");
        EXPECT_STATUS_OK(status) << "my assertion failed";
      },
      "\nmy assertion failed");
}

TEST(ExpectOkTest, ExpectationFailedDescriptionStatusOr) {
  EXPECT_NONFATAL_FAILURE(
      {
        StatusOr<int> status_or(Status(StatusCode::kInternal, "oh no!"));
        EXPECT_STATUS_OK(status_or) << "my assertion failed";
      },
      "\nmy assertion failed");
}

TEST(StatusIsTest, OkStatus) {
  Status status;
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(status, StatusIs(StatusCode::kOk));
  EXPECT_THAT(status, StatusIs(_));
  EXPECT_THAT(status, StatusIs(StatusCode::kOk, IsEmpty()));
  EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatus) {
  Status status(StatusCode::kUnknown, "hello");
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, "hello"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, Eq("hello")));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, HasSubstr("ello")));
  EXPECT_THAT(status, StatusIs(_, AnyOf("hello", "goodbye")));
  EXPECT_THAT(status,
              StatusIs(AnyOf(StatusCode::kAborted, StatusCode::kUnknown), _));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, _));
  EXPECT_THAT(status, StatusIs(_, "hello"));
  EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatusNegation) {
  Status status(StatusCode::kNotFound, "not found");

  // code doesn't match
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kUnknown, "not found")));

  // message doesn't match
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kNotFound, "found")));

  // both don't match
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kCancelled, "goodbye")));

  // combine with a few other matchers
  EXPECT_THAT(status, Not(StatusIs(AnyOf(StatusCode::kInvalidArgument,
                                         StatusCode::kInternal))));
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kUnknown, Eq("goodbye"))));
  EXPECT_THAT(status, StatusIs(Not(StatusCode::kUnknown), Not(IsEmpty())));
}

TEST(StatusIsTest, OkStatusOr) {
  StatusOr<std::string> status("StatusOr string value");
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(status, StatusIs(StatusCode::kOk));
  EXPECT_THAT(status, StatusIs(_));
  EXPECT_THAT(status, StatusIs(StatusCode::kOk, IsEmpty()));
  EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatusOr) {
  StatusOr<int> status(Status(StatusCode::kUnknown, "hello"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, "hello"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, Eq("hello")));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, HasSubstr("ello")));
  EXPECT_THAT(status, StatusIs(_, AnyOf("hello", "goodbye")));
  EXPECT_THAT(status,
              StatusIs(AnyOf(StatusCode::kAborted, StatusCode::kUnknown), _));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, _));
  EXPECT_THAT(status, StatusIs(_, "hello"));
  EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatusOrNegation) {
  StatusOr<float> status(Status(StatusCode::kNotFound, "not found"));

  // code doesn't match
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kUnknown, "not found")));

  // message doesn't match
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kNotFound, "found")));

  // both don't match
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kCancelled, "goodbye")));

  // combine with a few other matchers
  EXPECT_THAT(status, Not(StatusIs(AnyOf(StatusCode::kInvalidArgument,
                                         StatusCode::kInternal))));
  EXPECT_THAT(status, Not(StatusIs(StatusCode::kUnknown, Eq("goodbye"))));
  EXPECT_THAT(status, StatusIs(Not(StatusCode::kUnknown), Not(IsEmpty())));
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
