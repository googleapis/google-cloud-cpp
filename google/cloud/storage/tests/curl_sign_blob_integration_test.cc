// Copyright 2019 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_service_account;

class CurlSignBlobIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(CurlSignBlobIntegrationTest, Simple) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string service_account = flag_service_account;

  auto encoded = Base64Encode(LoremIpsum());

  SignBlobRequest request(service_account, encoded, {});

  StatusOr<SignBlobResponse> response = client->raw_client()->SignBlob(request);
  ASSERT_STATUS_OK(response);

  EXPECT_FALSE(response->key_id.empty());
  EXPECT_FALSE(response->signed_blob.empty());

  auto decoded = Base64Decode(response->signed_blob);
  EXPECT_FALSE(decoded.empty());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <service-account>\n";
    return 1;
  }

  google::cloud::storage::internal::flag_service_account = argv[1];

  return RUN_ALL_TESTS();
}
