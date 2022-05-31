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

#include "google/cloud/storage/internal/async_connection_impl.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::internal::OptionsSpan;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class AsyncConnectionImplTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kAuthority = "storage.googleapis.com";

std::shared_ptr<AsyncConnection> MakeTestConnection(
    CompletionQueue cq, std::shared_ptr<storage::testing::MockStorageStub> mock,
    Options options = {}) {
  return MakeAsyncConnection(
      std::move(cq), std::move(mock),
      storage::internal::DefaultOptionsGrpc(std::move(options)));
}

TEST_F(AsyncConnectionImplTest, AsyncDeleteObject) {
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject)
      .WillOnce(
          [this](CompletionQueue&, std::unique_ptr<grpc::ClientContext> context,
                 google::storage::v2::DeleteObjectRequest const& request) {
            EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
            auto metadata = GetMetadata(*context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
            EXPECT_THAT(request.object(), "test-object");
            return make_ready_future(PermanentError());
          });
  CompletionQueue cq;
  auto connection = MakeTestConnection(cq, mock);
  OptionsSpan span(connection->options());
  auto response =
      connection
          ->AsyncDeleteObject(
              storage::internal::DeleteObjectRequest("test-bucket",
                                                     "test-object")
                  .set_multiple_options(storage::Fields("field1,field2"),
                                        storage::QuotaUser("test-quota-user")))
          .get();
  EXPECT_THAT(response, StatusIs(PermanentError().code(),
                                 HasSubstr(PermanentError().message())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
