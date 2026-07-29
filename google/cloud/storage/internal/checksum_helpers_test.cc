// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/checksum_helpers.h"
#include "google/cloud/storage/internal/object_requests.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(ChecksumHelpersTest, DownloadChecksumSettingsDefaults) {
  auto settings = GetDownloadChecksumSettings(Options{});
  EXPECT_TRUE(settings.md5);
  EXPECT_FALSE(settings.crc32c);
}

TEST(ChecksumHelpersTest, DownloadChecksumSettingsOnlyNewOptions) {
  auto settings = GetDownloadChecksumSettings(
      Options{}.set<DownloadChecksumValidationOption>(ChecksumAlgorithm::kMD5));
  EXPECT_FALSE(settings.md5);
  EXPECT_TRUE(settings.crc32c);

  settings = GetDownloadChecksumSettings(
      Options{}.set<DownloadChecksumValidationOption>(
          ChecksumAlgorithm::kCrc32c));
  EXPECT_TRUE(settings.md5);
  EXPECT_FALSE(settings.crc32c);

  settings = GetDownloadChecksumSettings(
      Options{}.set<DownloadChecksumValidationOption>(
          ChecksumAlgorithm::kNone));
  EXPECT_TRUE(settings.md5);
  EXPECT_TRUE(settings.crc32c);
}

TEST(ChecksumHelpersTest, UploadChecksumSettingsDefaults) {
  auto settings = GetUploadChecksumSettings(Options{});
  EXPECT_TRUE(settings.md5);
  EXPECT_FALSE(settings.crc32c);
}

TEST(ChecksumHelpersTest, UploadChecksumSettingsOnlyNewOptions) {
  auto settings = GetUploadChecksumSettings(
      Options{}.set<UploadChecksumValidationOption>(ChecksumAlgorithm::kMD5));
  EXPECT_FALSE(settings.md5);
  EXPECT_TRUE(settings.crc32c);

  settings =
      GetUploadChecksumSettings(Options{}.set<UploadChecksumValidationOption>(
          ChecksumAlgorithm::kCrc32c));
  EXPECT_TRUE(settings.md5);
  EXPECT_FALSE(settings.crc32c);

  settings = GetUploadChecksumSettings(
      Options{}.set<UploadChecksumValidationOption>(ChecksumAlgorithm::kNone));
  EXPECT_TRUE(settings.md5);
  EXPECT_TRUE(settings.crc32c);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
