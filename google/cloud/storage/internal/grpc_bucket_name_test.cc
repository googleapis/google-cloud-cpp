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

#include "google/cloud/storage/internal/grpc_bucket_name.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(GrpcBucketName, ToGrpcBucketName) {
  EXPECT_EQ("projects/_/buckets/bucket-name",
            GrpcBucketIdToName("bucket-name"));
  EXPECT_EQ("projects/_/buckets/bucket.example.com",
            GrpcBucketIdToName("bucket.example.com"));
}

TEST(GrpcBucketName, FromGrpcBucketName) {
  EXPECT_EQ("bucket-name", GrpcBucketNameToId("bucket-name"));
  EXPECT_EQ("bucket-name",
            GrpcBucketNameToId("projects/_/buckets/bucket-name"));
  EXPECT_EQ("bucket.example.com",
            GrpcBucketNameToId("projects/_/buckets/bucket.example.com"));
}

TEST(GrpcBucketName, Roundtrip) {
  EXPECT_EQ("bucket-name",
            GrpcBucketNameToId(GrpcBucketIdToName("bucket-name")));
  EXPECT_EQ("bucket.example.com",
            GrpcBucketNameToId(GrpcBucketIdToName("bucket.example.com")));
};

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
