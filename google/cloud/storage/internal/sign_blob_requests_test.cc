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

#include "google/cloud/storage/internal/sign_blob_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(SignBlobRequestsTest, IOStream) {
  SignBlobRequest request("test-sa1", "blob-to-sign", {"test-sa2, test-sa3"});

  std::ostringstream os;
  os << request;
  auto actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("SignBlobRequest"));
  EXPECT_THAT(actual, HasSubstr("service_account=test-sa1"));
  EXPECT_THAT(actual, HasSubstr("base64_encoded_blob=blob-to-sign"));
  EXPECT_THAT(actual, HasSubstr("test-sa2"));
  EXPECT_THAT(actual, HasSubstr("test-sa3"));
}

TEST(SignBlobRequestsTest, ResponseParse) {
  std::string const resource_text = R"""({
      "keyId": "test-key-id",
      "signedBlob": "test-signed-blob"
})""";

  auto actual = SignBlobResponse::FromHttpResponse(resource_text).value();
  EXPECT_EQ("test-key-id", actual.key_id);
  EXPECT_EQ("test-signed-blob", actual.signed_blob);
}

TEST(SignBlobRequestsTest, ResponseParseFailure) {
  std::string text = R"""({123)""";

  auto actual = SignBlobResponse::FromHttpResponse(text);
  EXPECT_FALSE(actual.ok());
}

TEST(SignBlobRequestsTest, ResponseIOStream) {
  std::string const text = R"""({
      "keyId": "test-key-id",
      "signedBlob": "test-signed-blob"
})""";

  auto parsed = SignBlobResponse::FromHttpResponse(text).value();

  std::ostringstream os;
  os << parsed;
  std::string actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("SignBlobResponse"));
  EXPECT_THAT(actual, HasSubstr("key_id=test-key-id"));
  EXPECT_THAT(actual, HasSubstr("signed_blob=test-signed-blob"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
