// Copyright 2018 Google LLC
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

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(ComputeMD5HashTest, Empty) {
  std::string actual = ComputeMD5Hash("");
  // I used this command to get the expected value:
  // /bin/echo -n "" | openssl md5 -binary | openssl base64
  EXPECT_EQ("1B2M2Y8AsgTpgAmY7PhCfg==", actual);
}

TEST(ComputeMD5HashTest, Simple) {
  std::string actual =
      ComputeMD5Hash("The quick brown fox jumps over the lazy dog");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
  EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", actual);
}

TEST(ComputeCrc32cChecksumTest, Empty) {
  std::string actual = ComputeCrc32cChecksum("");
  // Use this command to get the expected value:
  // echo -n '' > foo.txt && gcloud storage hash foo.txt
  EXPECT_EQ("AAAAAA==", actual);
}

TEST(ComputeCrc32cChecksumTest, Simple) {
  std::string actual =
      ComputeCrc32cChecksum("The quick brown fox jumps over the lazy dog");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" > foo.txt &&
  // gcloud storage hash foo.txt
  EXPECT_EQ("ImIEBA==", actual);
}

TEST(ChecksumOptionsTest, SetAndGet) {
  Options options;
  EXPECT_FALSE(options.has<UploadChecksumValidationOption>());
  EXPECT_FALSE(options.has<DownloadChecksumValidationOption>());

  options.set<UploadChecksumValidationOption>(ChecksumAlgorithm::kMD5);
  options.set<DownloadChecksumValidationOption>(ChecksumAlgorithm::kCrc32c);

  EXPECT_TRUE(options.has<UploadChecksumValidationOption>());
  EXPECT_EQ(ChecksumAlgorithm::kMD5,
            options.get<UploadChecksumValidationOption>());

  EXPECT_TRUE(options.has<DownloadChecksumValidationOption>());
  EXPECT_EQ(ChecksumAlgorithm::kCrc32c,
            options.get<DownloadChecksumValidationOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
#include "google/cloud/internal/diagnostics_pop.inc"
