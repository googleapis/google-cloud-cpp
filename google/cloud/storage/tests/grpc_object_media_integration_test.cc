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

#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::IsEmpty;
using ::testing::Not;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcObjectMediaIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcObjectMediaIntegrationTest, CancelResumableUpload) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  ASSERT_THAT(bucket_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is not set";

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // Start an upload, capture its upload ID and suspend it.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  auto const upload_id = os.resumable_session_id();
  std::move(os).Suspend();

  auto status = client.DeleteResumableUpload(upload_id);
  EXPECT_STATUS_OK(status);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
