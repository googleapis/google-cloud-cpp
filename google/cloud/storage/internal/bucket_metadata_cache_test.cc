// Copyright 2026 Google LLC
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

#include "google/cloud/storage/internal/bucket_metadata_cache.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;

TEST(BucketMetadataCacheTest, HitAndMiss) {
  BucketMetadataCache cache(10);
  EXPECT_FALSE(cache.Get("test-bucket").has_value());

  BucketCacheEntry entry{"projects/123/buckets/test-bucket", "us-central1"};
  cache.Put("test-bucket", entry);

  auto res = cache.Get("test-bucket");
  ASSERT_TRUE(res.has_value());
  EXPECT_THAT(res->id, Eq("projects/123/buckets/test-bucket"));
  EXPECT_THAT(res->location, Eq("us-central1"));
}

TEST(BucketMetadataCacheTest, PutUpdatesExisting) {
  BucketMetadataCache cache(10);
  BucketCacheEntry entry1{"projects/123/buckets/test-bucket", "us-central1"};
  cache.Put("test-bucket", entry1);

  BucketCacheEntry entry2{"projects/456/buckets/test-bucket", "global"};
  cache.Put("test-bucket", entry2);

  auto res = cache.Get("test-bucket");
  ASSERT_TRUE(res.has_value());
  EXPECT_THAT(res->id, Eq("projects/456/buckets/test-bucket"));
  EXPECT_THAT(res->location, Eq("global"));
}

TEST(BucketMetadataCacheTest, InvalidateAndClear) {
  BucketMetadataCache cache(10);
  BucketCacheEntry entry{"projects/123/buckets/test-bucket", "us-central1"};
  cache.Put("test-bucket", entry);
  EXPECT_TRUE(cache.Get("test-bucket").has_value());

  cache.Invalidate("test-bucket");
  EXPECT_FALSE(cache.Get("test-bucket").has_value());

  cache.Put("bucket1", entry);
  cache.Put("bucket2", entry);
  EXPECT_TRUE(cache.Get("bucket1").has_value());
  EXPECT_TRUE(cache.Get("bucket2").has_value());

  cache.Clear();
  EXPECT_FALSE(cache.Get("bucket1").has_value());
  EXPECT_FALSE(cache.Get("bucket2").has_value());
}

TEST(BucketMetadataCacheTest, EvictsOldest) {
  BucketMetadataCache cache(2);
  BucketCacheEntry entry{"id", "loc"};

  cache.Put("b1", entry);
  cache.Put("b2", entry);
  EXPECT_TRUE(cache.Get("b1").has_value());  // b1 becomes most recent

  cache.Put("b3", entry);  // pushes out b2 (oldest)
  EXPECT_TRUE(cache.Get("b1").has_value());
  EXPECT_FALSE(cache.Get("b2").has_value());
  EXPECT_TRUE(cache.Get("b3").has_value());
}

TEST(BucketMetadataCacheTest, InFlightFetch) {
  BucketMetadataCache cache(10);
  EXPECT_THAT(cache.StartFetch("b1"), IsTrue());
  EXPECT_THAT(cache.StartFetch("b1"), IsFalse());

  EXPECT_THAT(cache.StartFetch("b2"), IsTrue());

  cache.EndFetch("b1");
  EXPECT_THAT(cache.StartFetch("b1"), IsTrue());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
