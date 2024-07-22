// Copyright 2019 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/base64.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

class CurlSignBlobIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    service_account_ =
        google::cloud::internal::GetEnv(
            "GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT")
            .value_or("");
    ASSERT_FALSE(service_account_.empty());
  }

  std::string service_account_;
};

TEST_F(CurlSignBlobIntegrationTest, Simple) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();
  auto encoded = Base64Encode(LoremIpsum());
  SignBlobRequest request(service_account_, encoded, {});

  // This is normally done by `storage::Client`, but we are bypassing it as part
  // of this test.
  auto connection = internal::ClientImplDetails::GetConnection(client);
  google::cloud::internal::OptionsSpan const span(connection->options());
  StatusOr<SignBlobResponse> response = connection->SignBlob(request);
  ASSERT_STATUS_OK(response);

  EXPECT_FALSE(response->key_id.empty());
  EXPECT_FALSE(response->signed_blob.empty());

  auto decoded = Base64Decode(response->signed_blob).value();
  EXPECT_FALSE(decoded.empty());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
