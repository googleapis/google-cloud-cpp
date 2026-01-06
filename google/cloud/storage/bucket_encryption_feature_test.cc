// Copyright 2025 Google LLC
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

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(BucketEncryptionTest, Parse) {
  std::string text = R"""({
      "encryption": {
        "defaultKmsKeyName": "projects/test-project-name/locations/us-central1/keyRings/test-keyring-name/cryptoKeys/test-key-name",
        "googleManagedEncryptionEnforcementConfig": {
          "restrictionMode": "FullyRestricted",
          "effectiveTime": "2025-12-18T18:13:15Z"
        },
        "customerManagedEncryptionEnforcementConfig": {
          "restrictionMode": "NotRestricted",
          "effectiveTime": "2025-12-18T18:13:15Z"
        },
        "customerSuppliedEncryptionEnforcementConfig": {
          "restrictionMode": "NotRestricted",
          "effectiveTime": "2025-12-18T18:13:15Z"
        }
      }
})""";

  auto actual = internal::BucketMetadataParser::FromString(text);
  ASSERT_STATUS_OK(actual);

  ASSERT_TRUE(actual->has_encryption());
  auto const& encryption = actual->encryption();
  EXPECT_EQ(
      "projects/test-project-name/locations/us-central1/keyRings/"
      "test-keyring-name/cryptoKeys/test-key-name",
      encryption.default_kms_key_name);

  EXPECT_EQ(
      "FullyRestricted",
      encryption.google_managed_encryption_enforcement_config.restriction_mode);
  EXPECT_EQ("2025-12-18T18:13:15Z",
            google::cloud::internal::FormatRfc3339(
                encryption.google_managed_encryption_enforcement_config
                    .effective_time));

  EXPECT_EQ("NotRestricted",
            encryption.customer_managed_encryption_enforcement_config
                .restriction_mode);
  EXPECT_EQ("2025-12-18T18:13:15Z",
            google::cloud::internal::FormatRfc3339(
                encryption.customer_managed_encryption_enforcement_config
                    .effective_time));

  EXPECT_EQ("NotRestricted",
            encryption.customer_supplied_encryption_enforcement_config
                .restriction_mode);
  EXPECT_EQ("2025-12-18T18:13:15Z",
            google::cloud::internal::FormatRfc3339(
                encryption.customer_supplied_encryption_enforcement_config
                    .effective_time));
}

TEST(BucketEncryptionTest, ToJson) {
  BucketMetadata meta;
  BucketEncryption encryption;
  encryption.default_kms_key_name = "test-key";
  encryption.google_managed_encryption_enforcement_config.restriction_mode =
      "FullyRestricted";
  encryption.google_managed_encryption_enforcement_config.effective_time =
      google::cloud::internal::ParseRfc3339("2025-12-18T18:13:15Z").value();
  encryption.customer_managed_encryption_enforcement_config.restriction_mode =
      "NotRestricted";
  encryption.customer_managed_encryption_enforcement_config.effective_time =
      google::cloud::internal::ParseRfc3339("2025-12-18T18:13:15Z").value();

  meta.set_encryption(encryption);

  auto json_string = internal::BucketMetadataToJsonString(meta);
  auto json = nlohmann::json::parse(json_string);

  ASSERT_TRUE(json.contains("encryption"));
  auto e = json["encryption"];
  EXPECT_EQ("test-key", e["defaultKmsKeyName"]);
  EXPECT_EQ("FullyRestricted",
            e["googleManagedEncryptionEnforcementConfig"]["restrictionMode"]);
  EXPECT_EQ("2025-12-18T18:13:15Z",
            e["googleManagedEncryptionEnforcementConfig"]["effectiveTime"]);
  EXPECT_EQ("NotRestricted",
            e["customerManagedEncryptionEnforcementConfig"]["restrictionMode"]);
  EXPECT_EQ("2025-12-18T18:13:15Z",
            e["customerManagedEncryptionEnforcementConfig"]["effectiveTime"]);
  EXPECT_FALSE(e.contains("customerSuppliedEncryptionEnforcementConfig"));
}

TEST(BucketEncryptionTest, Patch) {
  BucketMetadataPatchBuilder builder;
  BucketEncryption encryption;
  encryption.default_kms_key_name = "test-key";
  encryption.google_managed_encryption_enforcement_config.restriction_mode =
      "FullyRestricted";
  encryption.google_managed_encryption_enforcement_config.effective_time =
      google::cloud::internal::ParseRfc3339("2025-12-18T18:13:15Z").value();

  builder.SetEncryption(encryption);

  auto patch_string = builder.BuildPatch();
  auto patch = nlohmann::json::parse(patch_string);

  ASSERT_TRUE(patch.contains("encryption"));
  auto e = patch["encryption"];
  EXPECT_EQ("test-key", e["defaultKmsKeyName"]);
  EXPECT_EQ("FullyRestricted",
            e["googleManagedEncryptionEnforcementConfig"]["restrictionMode"]);
  EXPECT_EQ("2025-12-18T18:13:15Z",
            e["googleManagedEncryptionEnforcementConfig"]["effectiveTime"]);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
