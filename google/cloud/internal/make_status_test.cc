// Copyright 2022 Google LLC
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

#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::Contains;
using ::testing::Pair;

TEST(MakeStatus, Basic) {
  auto error_info = ErrorInfo("REASON", "domain", {{"key", "value"}});

  struct TestCase {
    StatusCode code;
    Status status;
  } cases[] = {
      {StatusCode::kCancelled, CancelledError("test", error_info)},
      {StatusCode::kUnknown, UnknownError("test", error_info)},
      {StatusCode::kInvalidArgument, InvalidArgumentError("test", error_info)},
      {StatusCode::kDeadlineExceeded,
       DeadlineExceededError("test", error_info)},
      {StatusCode::kNotFound, NotFoundError("test", error_info)},
      {StatusCode::kAlreadyExists, AlreadyExistsError("test", error_info)},
      {StatusCode::kPermissionDenied,
       PermissionDeniedError("test", error_info)},
      {StatusCode::kUnauthenticated, UnauthenticatedError("test", error_info)},
      {StatusCode::kResourceExhausted,
       ResourceExhaustedError("test", error_info)},
      {StatusCode::kFailedPrecondition,
       FailedPreconditionError("test", error_info)},
      {StatusCode::kAborted, AbortedError("test", error_info)},
      {StatusCode::kOutOfRange, OutOfRangeError("test", error_info)},
      {StatusCode::kUnimplemented, UnimplementedError("test", error_info)},
      {StatusCode::kInternal, InternalError("test", error_info)},
      {StatusCode::kUnavailable, UnavailableError("test", error_info)},
      {StatusCode::kDataLoss, DataLossError("test", error_info)},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for " + StatusCodeToString(test.code));
    EXPECT_THAT(test.status, StatusIs(test.code));
    EXPECT_EQ(test.status.message(), "test");
    EXPECT_EQ(test.status.error_info(), error_info);
  }
}

TEST(MakeStatus, ErrorInfoBuilderDefault) {
  auto const code = StatusCode::kInvalidArgument;
  auto const actual = GCP_ERROR_INFO().Build(code);
  EXPECT_EQ(actual.reason(), StatusCodeToString(code));
  EXPECT_EQ(actual.domain(), "gcloud-cpp");
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.version", version_string())));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.filename", __FILE__)));
  EXPECT_THAT(actual.metadata(), Contains(Pair("gcloud-cpp.source.line", _)));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.function", _)));
}

TEST(MakeStatus, ErrorInfoBuilderWithReason) {
  auto const code = StatusCode::kInvalidArgument;
  auto const actual = GCP_ERROR_INFO().WithReason("TEST_REASON").Build(code);
  EXPECT_EQ(actual.reason(), "TEST_REASON");
  EXPECT_EQ(actual.domain(), "gcloud-cpp");
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.version", version_string())));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.filename", __FILE__)));
  EXPECT_THAT(actual.metadata(), Contains(Pair("gcloud-cpp.source.line", _)));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.function", _)));
}

TEST(MakeStatus, ErrorInfoBuilderWithErrorContext) {
  auto ec1 = ErrorContext({{"k0", "v0"}, {"k1", "v1"}});
  auto ec2 = ErrorContext({{"k0", "not-used"}, {"k2", "v2"}});

  auto const code = StatusCode::kInvalidArgument;
  auto const actual =
      GCP_ERROR_INFO().WithContext(ec1).WithContext(ec2).Build(code);
  EXPECT_THAT(actual.metadata(), Contains(Pair("k0", "v0")));
  EXPECT_THAT(actual.metadata(), Contains(Pair("k1", "v1")));
  EXPECT_THAT(actual.metadata(), Contains(Pair("k2", "v2")));
  EXPECT_EQ(actual.reason(), StatusCodeToString(code));
  EXPECT_EQ(actual.domain(), "gcloud-cpp");
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.version", version_string())));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.filename", __FILE__)));
  EXPECT_THAT(actual.metadata(), Contains(Pair("gcloud-cpp.source.line", _)));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.function", _)));
}

TEST(MakeStatus, ErrorInfoBuilderWithMetadata) {
  auto const code = StatusCode::kInvalidArgument;
  auto const actual = GCP_ERROR_INFO()
                          .WithMetadata("k0", "v0")
                          .WithMetadata("k1", "v1")
                          .WithMetadata("k0", "not-used")
                          .Build(code);
  EXPECT_THAT(actual.metadata(), Contains(Pair("k0", "v0")));
  EXPECT_THAT(actual.metadata(), Contains(Pair("k1", "v1")));
  EXPECT_EQ(actual.reason(), StatusCodeToString(code));
  EXPECT_EQ(actual.domain(), "gcloud-cpp");
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.version", version_string())));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.filename", __FILE__)));
  EXPECT_THAT(actual.metadata(), Contains(Pair("gcloud-cpp.source.line", _)));
  EXPECT_THAT(actual.metadata(),
              Contains(Pair("gcloud-cpp.source.function", _)));
}

TEST(MakeStatus, WithErrorInfo) {
  struct TestCase {
    StatusCode code;
    Status status;
  } cases[] = {
      {StatusCode::kCancelled, CancelledError("test", GCP_ERROR_INFO())},
      {StatusCode::kUnknown, UnknownError("test", GCP_ERROR_INFO())},
      {StatusCode::kInvalidArgument,
       InvalidArgumentError("test", GCP_ERROR_INFO())},
      {StatusCode::kDeadlineExceeded,
       DeadlineExceededError("test", GCP_ERROR_INFO())},
      {StatusCode::kNotFound, NotFoundError("test", GCP_ERROR_INFO())},
      {StatusCode::kAlreadyExists,
       AlreadyExistsError("test", GCP_ERROR_INFO())},
      {StatusCode::kPermissionDenied,
       PermissionDeniedError("test", GCP_ERROR_INFO())},
      {StatusCode::kUnauthenticated,
       UnauthenticatedError("test", GCP_ERROR_INFO())},
      {StatusCode::kResourceExhausted,
       ResourceExhaustedError("test", GCP_ERROR_INFO())},
      {StatusCode::kFailedPrecondition,
       FailedPreconditionError("test", GCP_ERROR_INFO())},
      {StatusCode::kAborted, AbortedError("test", GCP_ERROR_INFO())},
      {StatusCode::kOutOfRange, OutOfRangeError("test", GCP_ERROR_INFO())},
      {StatusCode::kUnimplemented,
       UnimplementedError("test", GCP_ERROR_INFO())},
      {StatusCode::kInternal, InternalError("test", GCP_ERROR_INFO())},
      {StatusCode::kUnavailable, UnavailableError("test", GCP_ERROR_INFO())},
      {StatusCode::kDataLoss, DataLossError("test", GCP_ERROR_INFO())},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for " + StatusCodeToString(test.code));
    EXPECT_THAT(test.status, StatusIs(test.code));
    EXPECT_EQ(test.status.message(), "test");
    EXPECT_EQ(test.status.error_info().reason(), StatusCodeToString(test.code));
    EXPECT_EQ(test.status.error_info().domain(), "gcloud-cpp");
    EXPECT_THAT(test.status.error_info().metadata(),
                Contains(Pair("gcloud-cpp.version", _)));
    EXPECT_THAT(test.status.error_info().metadata(),
                Contains(Pair("gcloud-cpp.source.filename", __FILE__)));
    EXPECT_THAT(test.status.error_info().metadata(),
                Contains(Pair("gcloud-cpp.source.line", _)));
    EXPECT_THAT(test.status.error_info().metadata(),
                Contains(Pair("gcloud-cpp.source.function", _)));
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
