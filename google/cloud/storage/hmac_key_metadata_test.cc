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

#include "google/cloud/storage/hmac_key_metadata.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/storage/internal/hmac_key_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

HmacKeyMetadata CreateHmacKeyMetadataForTest() {
  std::string text = R"""({
      "accessId": "test-access-id",
      "etag": "XYZ=",
      "id": "test-id-123",
      "kind": "storage#hmacKeyMetadata",
      "projectId": "test-project-id",
      "serviceAccountEmail": "test-service-account-email",
      "state": "ACTIVE",
      "timeCreated": "2019-03-01T12:13:14Z",
      "updated": "2019-03-02T12:13:14Z"
})""";
  return internal::HmacKeyMetadataParser::FromString(text).value();
}

/// @test Verifies HmacKeyMetadata parsing.
TEST(HmacKeyMetadataTest, Parser) {
  auto hmac = CreateHmacKeyMetadataForTest();

  EXPECT_EQ("test-access-id", hmac.access_id());
  EXPECT_EQ("XYZ=", hmac.etag());
  EXPECT_EQ("test-id-123", hmac.id());
  EXPECT_EQ("storage#hmacKeyMetadata", hmac.kind());
  EXPECT_EQ("test-project-id", hmac.project_id());
  EXPECT_EQ("test-service-account-email", hmac.service_account_email());
  EXPECT_EQ(HmacKeyMetadata::state_active(), hmac.state());
  EXPECT_EQ("2019-03-01T12:13:14Z",
            google::cloud::internal::FormatRfc3339(hmac.time_created()));
  EXPECT_EQ("2019-03-02T12:13:14Z",
            google::cloud::internal::FormatRfc3339(hmac.updated()));

  EXPECT_EQ("ACTIVE", HmacKeyMetadata::state_active());
  EXPECT_EQ("INACTIVE", HmacKeyMetadata::state_inactive());
  EXPECT_EQ("DELETED", HmacKeyMetadata::state_deleted());
}

/// @test Verifies HmacKeyMetadata iostream operator.
TEST(HmacKeyMetadataTest, IOStream) {
  auto hmac = CreateHmacKeyMetadataForTest();

  std::ostringstream os;
  os << hmac;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-access-id"));
  EXPECT_THAT(actual, HasSubstr("test-id-123"));
  EXPECT_THAT(actual, HasSubstr("test-project-id"));
  EXPECT_THAT(actual, HasSubstr("test-service-account-email"));
  EXPECT_THAT(actual, HasSubstr("ACTIVE"));
  EXPECT_THAT(actual, HasSubstr("2019-03-01T12:13:14Z"));
  EXPECT_THAT(actual, HasSubstr("2019-03-02T12:13:14Z"));
}

/// @test Verify we can change the state in a HmacKeyMetadata.
TEST(HmacKeyMetadataTest, SetState) {
  auto expected = CreateHmacKeyMetadataForTest();
  auto copy = expected;
  copy.set_state("INACTIVE");
  EXPECT_EQ("INACTIVE", copy.state());
  EXPECT_NE(expected.state(), copy.state());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the state in a HmacKeyMetadata.
TEST(HmacKeyMetadataTest, SetETag) {
  auto expected = CreateHmacKeyMetadataForTest();
  auto copy = expected;
  copy.set_etag("ABC=");
  EXPECT_EQ("ABC=", copy.etag());
  EXPECT_NE(expected.etag(), copy.etag());
  EXPECT_NE(expected, copy);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
