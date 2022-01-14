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

#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;

// All the tests have nearly identical structure. They create a stub pointing to
// an invalid endpoint. Getting a kUnavailable error is the expected result.

std::shared_ptr<StorageStub> CreateTestStub(CompletionQueue cq) {
  auto credentials = google::cloud::MakeAccessTokenCredentials(
      "test-only-invalid",
      std::chrono::system_clock::now() + std::chrono::minutes(5));
  return CreateStorageStub(std::move(cq),
                           google::cloud::Options{}
                               .set<EndpointOption>("localhost:1")
                               .set<TracingComponentsOption>({"rpc"})
                               .set<UnifiedCredentialsOption>(credentials));
}

TEST(StorageStubFactory, ReadObject) {
  CompletionQueue cq;
  auto stub = CreateTestStub(cq);
  auto stream = stub->ReadObject(absl::make_unique<grpc::ClientContext>(),
                                 google::storage::v2::ReadObjectRequest{});
  auto read = stream->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(read));
  EXPECT_THAT(absl::get<Status>(read), StatusIs(StatusCode::kUnavailable));
}

TEST(StorageStubFactory, WriteObject) {
  CompletionQueue cq;
  auto stub = CreateTestStub(cq);
  auto stream = stub->WriteObject(absl::make_unique<grpc::ClientContext>());
  auto close = stream->Close();
  EXPECT_THAT(close, StatusIs(StatusCode::kUnavailable));
}

TEST(StorageStubFactory, StartResumableWrite) {
  CompletionQueue cq;
  auto stub = CreateTestStub(cq);
  grpc::ClientContext context;
  auto response = stub->StartResumableWrite(
      context, google::storage::v2::StartResumableWriteRequest{});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST(StorageStubFactory, QueryWriteStatus) {
  CompletionQueue cq;
  auto stub = CreateTestStub(cq);
  grpc::ClientContext context;
  auto response = stub->QueryWriteStatus(
      context, google::storage::v2::QueryWriteStatusRequest{});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
