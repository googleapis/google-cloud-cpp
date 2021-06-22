// Copyright 2021 Google LLC
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

#include "google/cloud/internal/extract_long_running_result.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::bigtable::admin::v2::Instance;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::longrunning::Operation;
using ::testing::HasSubstr;

TEST(ExtractLongRunningResultTest, MetadataDoneWithSuccess) {
  Instance expected;
  expected.set_name("test-instance-admin");
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_metadata()->PackFrom(expected);
  auto const actual =
      ExtractLongRunningResultMetadata<Instance>(op, "test-function");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(ExtractLongRunningResultTest, MetadataDoneWithError) {
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_error()->set_code(grpc::StatusCode::PERMISSION_DENIED);
  op.mutable_error()->set_message("uh-oh");
  auto const actual =
      ExtractLongRunningResultMetadata<Instance>(op, "test-function");
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(ExtractLongRunningResultTest, MetadataDoneWithoutResult) {
  google::longrunning::Operation op;
  op.set_done(true);
  auto const actual =
      ExtractLongRunningResultMetadata<Instance>(op, "test-function");
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal));
}

TEST(ExtractLongRunningResultTest, MetadataDoneWithInvalidContent) {
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_metadata()->PackFrom(google::protobuf::Empty{});
  auto const actual =
      ExtractLongRunningResultMetadata<Instance>(op, "test-function");
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("invalid metadata type"))));
}

TEST(ExtractLongRunningResultTest, MetadataError) {
  auto const expected = Status{StatusCode::kPermissionDenied, "uh-oh"};
  auto const actual =
      ExtractLongRunningResultMetadata<Instance>(expected, "test-function");
  ASSERT_THAT(actual, Not(IsOk()));
  EXPECT_EQ(expected, actual.status());
}

TEST(ExtractLongRunningResultTest, ResponseDoneWithSuccess) {
  Instance expected;
  expected.set_name("test-instance-admin");
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_response()->PackFrom(expected);
  auto const actual =
      ExtractLongRunningResultResponse<Instance>(op, "test-function");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(ExtractLongRunningResultTest, ResponseDoneWithError) {
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_error()->set_code(grpc::StatusCode::PERMISSION_DENIED);
  op.mutable_error()->set_message("uh-oh");
  auto const actual =
      ExtractLongRunningResultResponse<Instance>(op, "test-function");
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(ExtractLongRunningResultTest, ResponseDoneWithoutResult) {
  google::longrunning::Operation op;
  op.set_done(true);
  auto const actual =
      ExtractLongRunningResultResponse<Instance>(op, "test-function");
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal));
}

TEST(ExtractLongRunningResultTest, ResponseDoneWithInvalidContent) {
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_response()->PackFrom(google::protobuf::Empty{});
  auto const actual =
      ExtractLongRunningResultResponse<Instance>(op, "test-function");
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("invalid response type"))));
}

TEST(ExtractLongRunningResultTest, ResponseError) {
  auto const expected = Status{StatusCode::kPermissionDenied, "uh-oh"};
  auto const actual =
      ExtractLongRunningResultResponse<Instance>(expected, "test-function");
  ASSERT_THAT(actual, Not(IsOk()));
  EXPECT_EQ(expected, actual.status());
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
