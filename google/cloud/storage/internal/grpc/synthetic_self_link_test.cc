// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/grpc/synthetic_self_link.h"
#include "google/cloud/storage/options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::storage::RestEndpointOption;

TEST(SyntheticSelfLink, Root) {
  EXPECT_EQ("https://www.googleapis.com/storage/v1",
            SyntheticSelfLinkRoot(Options{}));
  EXPECT_EQ("https://www.googleapis.com/storage/v1",
            SyntheticSelfLinkRoot(Options{}.set<RestEndpointOption>(
                "https://storage.googleapis.com")));
  EXPECT_EQ("https://restricted.googleapis.com/storage/v1",
            SyntheticSelfLinkRoot(Options{}.set<RestEndpointOption>(
                "https://restricted.googleapis.com")));
  EXPECT_EQ("https://emulator:8080/storage/v1",
            SyntheticSelfLinkRoot(
                Options{}.set<RestEndpointOption>("https://emulator:8080")));
  EXPECT_EQ(
      "https://restricted.googleapis.com/storage/v7",
      SyntheticSelfLinkRoot(
          Options{}
              .set<RestEndpointOption>("https://restricted.googleapis.com")
              .set<storage::internal::TargetApiVersionOption>("v7")));
}

TEST(SyntheticSelfLink, DownloadRoot) {
  EXPECT_EQ("https://storage.googleapis.com/download/storage/v1",
            SyntheticSelfLinkDownloadRoot(Options{}));
  EXPECT_EQ("https://storage.googleapis.com/download/storage/v1",
            SyntheticSelfLinkDownloadRoot(Options{}.set<RestEndpointOption>(
                "https://storage.googleapis.com")));
  EXPECT_EQ("https://restricted.googleapis.com/download/storage/v1",
            SyntheticSelfLinkDownloadRoot(Options{}.set<RestEndpointOption>(
                "https://restricted.googleapis.com")));
  EXPECT_EQ("https://emulator:8080/download/storage/v7",
            SyntheticSelfLinkDownloadRoot(
                Options{}
                    .set<RestEndpointOption>("https://emulator:8080")
                    .set<storage::internal::TargetApiVersionOption>("v7")));
}

TEST(SyntheticSelfLink, Bucket) {
  EXPECT_EQ("https://www.googleapis.com/storage/v1/b/test-bucket",
            SyntheticSelfLinkBucket(Options{}, "test-bucket"));
  EXPECT_EQ("https://restricted.googleapis.com/storage/v1/b/test-bucket",
            SyntheticSelfLinkBucket(Options{}.set<RestEndpointOption>(
                                        "https://restricted.googleapis.com"),
                                    "test-bucket"));
}

TEST(SyntheticSelfLink, Object) {
  EXPECT_EQ("https://www.googleapis.com/storage/v1/b/test-bucket/o/test-object",
            SyntheticSelfLinkObject(Options{}, "test-bucket", "test-object"));
  EXPECT_EQ(
      "https://restricted.googleapis.com/storage/v1/b/test-bucket/o/"
      "test-object",
      SyntheticSelfLinkObject(Options{}.set<RestEndpointOption>(
                                  "https://restricted.googleapis.com"),
                              "test-bucket", "test-object"));
  EXPECT_EQ(
      "https://www.googleapis.com/storage/v1/b/test-bucket/o/"
      "d%2F%201%2F%3D%26%3F-object",
      SyntheticSelfLinkObject(Options{}, "test-bucket", "d/ 1/=&?-object"));
}

TEST(SyntheticSelfLink, Download) {
  EXPECT_EQ(
      "https://restricted.googleapis.com/download/storage/v1/b/test-bucket/o/"
      "test-object?generation=1234&alt=media",
      SyntheticSelfLinkDownload(Options{}.set<RestEndpointOption>(
                                    "https://restricted.googleapis.com"),
                                "test-bucket", "test-object", 1234));
  EXPECT_EQ(
      "https://storage.googleapis.com/download/storage/v1/b/test-bucket/o/"
      "test-object?generation=1234&alt=media",
      SyntheticSelfLinkDownload(Options{}, "test-bucket", "test-object", 1234));
  EXPECT_EQ(
      "https://storage.googleapis.com/download/storage/v1/b/test-bucket/o/"
      "d%2F%201%2F%3D%26%3F-object?generation=1234&alt=media",
      SyntheticSelfLinkDownload(Options{}, "test-bucket", "d/ 1/=&?-object",
                                1234));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
