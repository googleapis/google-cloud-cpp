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

#include "google/cloud/storage/idempotency_policy.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

TEST(StrictIdempotencyPolicyTest, ListBuckets) {
  StrictIdempotencyPolicy policy;
  internal::ListBucketsRequest request("test-project");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateBucket) {
  StrictIdempotencyPolicy policy;
  internal::CreateBucketRequest request(
      "test-project", BucketMetadata().set_name("test-bucket-name"));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetBucketMetadata) {
  StrictIdempotencyPolicy policy;
  internal::GetBucketMetadataRequest request("test-bucket-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucket) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketRequest request("test-bucket-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucketIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketRequest request("test-bucket-name");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucketIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketRequest request("test-bucket-name");
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucket) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketRequest request(
      BucketMetadata().set_name("test-bucket-name"));
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucketIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketRequest request(
      BucketMetadata().set_name("test-bucket-name"));
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucketIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketRequest request(
      BucketMetadata().set_name("test-bucket-name"));
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucket) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketRequest request("test-bucket-name",
                                       BucketMetadataPatchBuilder());
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucketIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketRequest request("test-bucket-name",
                                       BucketMetadataPatchBuilder());
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucketIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketRequest request("test-bucket-name",
                                       BucketMetadataPatchBuilder());
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
