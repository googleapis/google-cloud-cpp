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

#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::MockFunction;
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

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersEmpty) {
  grpc::ClientContext context;
  ApplyQueryParameters(context,
                       ReadObjectRangeRequest("test-bucket", "test-object"));
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, IsEmpty());
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersWithFields) {
  grpc::ClientContext context;
  ApplyQueryParameters(
      context, ReadObjectRangeRequest("test-bucket", "test-object")
                   .set_option(Fields("bucket,name,generation,contentType")));
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, Contains(Pair("x-goog-fieldmask",
                                      "bucket,name,generation,contentType")));
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersWithFieldsAndPrefix) {
  grpc::ClientContext context;
  ApplyQueryParameters(
      context,
      InsertObjectMediaRequest("test-bucket", "test-object", "content")
          .set_option(Fields("bucket,name,generation,contentType")),
      "resource");
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata,
              Contains(Pair("x-goog-fieldmask",
                            "resource(bucket,name,generation,contentType)")));
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersQuotaUserAndUserIp) {
  struct {
    ReadObjectRangeRequest request;
    std::string expected;
  } cases[] = {
      {ReadObjectRangeRequest("b", "o").set_option(UserIp("1.2.3.4")),
       "1.2.3.4"},
      {ReadObjectRangeRequest("b", "o")
           .set_option(QuotaUser("test-only-quota-user"))
           .set_option(UserIp("1.2.3.4")),
       "test-only-quota-user"},
      {ReadObjectRangeRequest("b", "o").set_option(
           QuotaUser("test-only-quota-user")),
       "test-only-quota-user"},
  };

  for (auto const& test : cases) {
    auto description = [&test] {
      std::ostringstream os;
      os << "Testing with request=" << test.request;
      return std::move(os).str();
    };
    SCOPED_TRACE(description());
    grpc::ClientContext context;
    ApplyQueryParameters(context, test.request);
    auto metadata = GetMetadata(context);
    EXPECT_THAT(metadata, Contains(Pair("x-goog-quota-user", test.expected)));
  }
}

TEST_F(GrpcConfigureClientContext, ApplyQueryParametersGrpcOptions) {
  MockFunction<void(grpc::ClientContext&)> mock;
  EXPECT_CALL(mock, Call);

  google::cloud::internal::OptionsSpan span(
      Options{}.set<google::cloud::internal::GrpcSetupOption>(
          mock.AsStdFunction()));

  grpc::ClientContext context;
  ApplyQueryParameters(context,
                       ReadObjectRangeRequest("test-bucket", "test-object"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
