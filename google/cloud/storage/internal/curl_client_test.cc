// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::google::cloud::storage::Status;
using ::google::cloud::storage::oauth2::Credentials;

std::string const STATUS_ERROR_MSG =
    "FailingCredentials doing its job, failing";

// We create a credential class that always fails to fetch an access token; this
// allows us to check that CurlClient methods fail early when their setup steps
// (which include adding the authorization header) return a failure Status.
// Note that if the methods performing the setup steps were not templated, we
// could simply mock those out instead via MOCK_METHOD<N> macros.
class FailingCredentials : public Credentials {
 public:
  std::pair<Status, std::string> AuthorizationHeader() override {
    return std::make_pair(Status(503, STATUS_ERROR_MSG), "");
  }
};

class CurlClientTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    client_ = google::cloud::internal::make_unique<CurlClient>(
        std::make_shared<FailingCredentials>());
  }

  static std::unique_ptr<CurlClient> client_;
};
std::unique_ptr<CurlClient> CurlClientTest::client_ = nullptr;

void TestCorrectFailureStatus(Status const& status) {
  EXPECT_EQ(status.status_code(), 503);
  EXPECT_EQ(status.error_message(), STATUS_ERROR_MSG);
}

TEST_F(CurlClientTest, ListBuckets) {
  auto status_and_foo = client_->ListBuckets(ListBucketsRequest{"project_id"});
  TestCorrectFailureStatus(status_and_foo.first);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
