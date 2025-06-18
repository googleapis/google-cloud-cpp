// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/handle_redirect_error.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/status_payload_keys.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;

TEST(ExtractGrpcStatus, Success) {
  auto const actual = ExtractGrpcStatus(Status());
  EXPECT_EQ(actual.code(), 0);
  EXPECT_THAT(actual.message(), IsEmpty());
  EXPECT_THAT(actual.details(), IsEmpty());
}

TEST(ExtractGrpcStatus, GenericError) {
  google::rpc::Status rpc_status;
  rpc_status.set_code(static_cast<int>(StatusCode::kInternal));
  rpc_status.set_message("test-message");
  std::string rpc_status_payload;
  ASSERT_TRUE(rpc_status.SerializeToString(&rpc_status_payload));

  Status input(StatusCode::kInternal, "test-message");
  internal::SetPayload(input, internal::StatusPayloadGrpcProto(),
                       rpc_status_payload);

  auto const actual = ExtractGrpcStatus(input);

  EXPECT_EQ(actual.code(), rpc_status.code());
  EXPECT_EQ(actual.message(), rpc_status.message());
  EXPECT_THAT(actual.details(), IsEmpty());
}

TEST(ApplyRedirectErrors, NoRedirect) {
  google::storage::v2::BidiReadObjectSpec spec;
  spec.set_bucket("projects/_/buckets/test-bucket");
  spec.set_object("test-object");
  google::rpc::Status rpc_status;
  rpc_status.set_code(static_cast<int>(StatusCode::kNotFound));
  rpc_status.set_message("test-message");

  ApplyRedirectErrors(spec, rpc_status);
  EXPECT_EQ(spec.bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(spec.object(), "test-object");
  EXPECT_FALSE(spec.has_read_handle());
  EXPECT_TRUE(spec.routing_token().empty());
}

TEST(EnsureFirstMessageAppendObjectSpec, Success) {
  google::storage::v2::BidiWriteObjectRequest request;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        write_object_spec {
          resource { bucket: "projects/_/buckets/b", name: "o" }
          if_metageneration_match: 1
          if_metageneration_not_match: 1
        }
      )pb",
      &request));

  google::rpc::Status rpc_status;
  google::storage::v2::BidiWriteObjectRedirectedError redirect;
  redirect.mutable_write_handle();
  rpc_status.add_details()->PackFrom(redirect);

  EnsureFirstMessageAppendObjectSpec(request, rpc_status);

  EXPECT_FALSE(request.has_write_object_spec());
  EXPECT_TRUE(request.has_append_object_spec());

  auto const& append_spec = request.append_object_spec();
  EXPECT_EQ(append_spec.bucket(), "projects/_/buckets/b");
  EXPECT_EQ(append_spec.object(), "o");

  EXPECT_FALSE(append_spec.has_write_handle());
  EXPECT_TRUE(append_spec.routing_token().empty());
  EXPECT_EQ(append_spec.if_metageneration_match(), 1);
  EXPECT_EQ(append_spec.if_metageneration_not_match(), 1);
  EXPECT_EQ(append_spec.generation(), 0);
}

TEST(EnsureFirstMessageAppendObjectSpec, WriteHandleIsNotSet) {
  google::storage::v2::BidiWriteObjectRequest request;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        write_object_spec {
          resource { bucket: "projects/_/buckets/b", name: "o" }
        }
      )pb",
      &request));

  google::rpc::Status rpc_status;
  google::storage::v2::BidiWriteObjectRedirectedError redirect;
  redirect.set_generation(1234);
  rpc_status.add_details()->PackFrom(redirect);

  EnsureFirstMessageAppendObjectSpec(request, rpc_status);

  EXPECT_TRUE(request.has_write_object_spec());
  EXPECT_FALSE(request.has_append_object_spec());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
