// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/grpc/configure_client_context.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::CreateNullHashFunction;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::_;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Pair;

class GrpcConfigureClientContext : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(GrpcConfigureClientContext, AddIdempotencyToken) {
  grpc::ClientContext ctx;
  auto const context = rest_internal::RestContext{}.AddHeader(
      "x-goog-gcs-idempotency-token", "token-123");
  AddIdempotencyToken(ctx, context);
  auto metadata = GetMetadata(ctx);
  EXPECT_THAT(metadata,
              Contains(Pair("x-goog-gcs-idempotency-token", "token-123")));
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersEmpty) {
  grpc::ClientContext ctx;
  ApplyQueryParameters(
      ctx, Options{},
      storage::internal::ReadObjectRangeRequest("test-bucket", "test-object"));
  auto metadata = GetMetadata(ctx);
  EXPECT_THAT(metadata, IsEmpty());
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersWithFields) {
  grpc::ClientContext ctx;
  ApplyQueryParameters(
      ctx, Options{},
      storage::internal::ReadObjectRangeRequest("test-bucket", "test-object")
          .set_option(storage::Fields("bucket,name,generation,contentType")));
  auto metadata = GetMetadata(ctx);
  EXPECT_THAT(metadata, Contains(Pair("x-goog-fieldmask",
                                      "bucket,name,generation,contentType")));
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersWithFieldsAndPrefix) {
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, Options{},
                       storage::internal::InsertObjectMediaRequest(
                           "test-bucket", "test-object", "content")
                           .set_option(storage::Fields(
                               "resource.bucket,resource.content_type")));
  auto metadata = GetMetadata(ctx);
  EXPECT_THAT(metadata,
              Contains(Pair("x-goog-fieldmask",
                            "resource.bucket,resource.content_type")));
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersQuotaUserAndUserIp) {
  struct {
    storage::internal::ReadObjectRangeRequest request;
    std::string expected;
  } cases[] = {
      {storage::internal::ReadObjectRangeRequest("b", "o").set_option(
           storage::UserIp("1.2.3.4")),
       "1.2.3.4"},
      {storage::internal::ReadObjectRangeRequest("b", "o")
           .set_option(storage::QuotaUser("test-only-quota-user"))
           .set_option(storage::UserIp("1.2.3.4")),
       "test-only-quota-user"},
      {storage::internal::ReadObjectRangeRequest("b", "o").set_option(
           storage::QuotaUser("test-only-quota-user")),
       "test-only-quota-user"},
  };

  for (auto const& test : cases) {
    auto description = [&test] {
      std::ostringstream os;
      os << "Testing with request=" << test.request;
      return std::move(os).str();
    };
    SCOPED_TRACE(description());
    grpc::ClientContext ctx;
    ApplyQueryParameters(ctx, Options{}, test.request);
    auto metadata = GetMetadata(ctx);
    EXPECT_THAT(metadata, Contains(Pair("x-goog-quota-user", test.expected)));
  }
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersGrpcOptions) {
  MockFunction<void(grpc::ClientContext&)> mock;
  EXPECT_CALL(mock, Call);

  auto const options = Options{}.set<google::cloud::internal::GrpcSetupOption>(
      mock.AsStdFunction());

  grpc::ClientContext ctx;
  ApplyQueryParameters(
      ctx, options,
      storage::internal::ReadObjectRangeRequest("test-bucket", "test-object"));
}

TEST_F(GrpcConfigureClientContext, ApplyRoutingHeadersInsertObjectMedia) {
  storage::internal::InsertObjectMediaRequest req("test-bucket", "test-object",
                                                  "content");

  grpc::ClientContext context;
  ApplyRoutingHeaders(context, req);
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata,
              Contains(Pair("x-goog-request-params",
                            "bucket=projects%2F_%2Fbuckets%2Ftest-bucket")));
}

TEST_F(GrpcConfigureClientContext, ApplyRoutingHeadersInsertObject) {
  auto spec = google::storage::v2::WriteObjectSpec{};
  spec.mutable_resource()->set_bucket("projects/_/buckets/test-bucket");

  grpc::ClientContext context;
  ApplyRoutingHeaders(context, spec);
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata,
              Contains(Pair("x-goog-request-params",
                            "bucket=projects%2F_%2Fbuckets%2Ftest-bucket")));
}

TEST_F(GrpcConfigureClientContext, ApplyRoutingHeadersUploadChunkMatchSlash) {
  storage::internal::UploadChunkRequest req(
      "projects/_/buckets/test-bucket/blah/blah", 0, {},
      CreateNullHashFunction());

  grpc::ClientContext context;
  ApplyRoutingHeaders(context, req);
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata,
              Contains(Pair("x-goog-request-params",
                            "bucket=projects%2F_%2Fbuckets%2Ftest-bucket")));
}

TEST_F(GrpcConfigureClientContext, ApplyRoutingHeadersUploadChunkNoMatch) {
  storage::internal::UploadChunkRequest req("does-not-match", 0, {},
                                            CreateNullHashFunction());

  grpc::ClientContext context;
  ApplyRoutingHeaders(context, req);
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, Not(Contains(Pair("x-goog-request-params", _))));
}

TEST_F(GrpcConfigureClientContext, ApplyRoutingHeadersUploadId) {
  struct TestCase {
    std::string upload_id;
    std::string expected;
  } const cases[] = {
      {"projects/_/buckets/test-bucket/test-upload-id",
       "bucket=projects%2F_%2Fbuckets%2Ftest-bucket"},
      {"projects/_/buckets/test-bucket/test/upload/id",
       "bucket=projects%2F_%2Fbuckets%2Ftest-bucket"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with " + test.upload_id);
    grpc::ClientContext context;
    ApplyResumableUploadRoutingHeader(context, test.upload_id);
    auto metadata = GetMetadata(context);
    EXPECT_THAT(metadata,
                Contains(Pair("x-goog-request-params", test.expected)));
  }
}

TEST_F(GrpcConfigureClientContext, ApplyRoutingHeadersUploadIdNoMatch) {
  using ::testing::Eq;
  using ::testing::Matcher;

  grpc::ClientContext context;
  ApplyResumableUploadRoutingHeader(context, "not-a-match");
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, Not(Contains(Pair("x-goog-request-params", _))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
