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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/storage/testing/remove_stale_buckets.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/storagecontrol/v2/storage_control_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include "google/storage/control/v2/storage_control.pb.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/field_mask.pb.h>
#include <gmock/gmock.h>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storagecontrol_v2::MakeStorageControlConnection;
using ::google::cloud::storagecontrol_v2::StorageControlClient;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::Eq;

using RapidCache = ::google::storage::control::v2::RapidCache;

// The only cache type the service accepts; plain "rapid-cache" is rejected.
auto constexpr kCacheType = "rapid-cache-ultra";
auto constexpr kDefaultTtlSeconds = std::int64_t{24 * 60 * 60};
auto constexpr kUpdatedTtlSeconds = std::int64_t{48 * 60 * 60};

// Bounds the wait on a long-running operation so that a stuck operation fails
// this test instead of hanging the whole suite.
auto constexpr kLroTimeout = std::chrono::minutes(10);

// Every run creates its own buckets with this prefix, and removes buckets with
// this prefix that earlier runs failed to delete.
auto constexpr kBucketPrefix = "cloud-cpp-rcu";
auto constexpr kStaleBucketAge = std::chrono::hours(48);

// Deleting a bucket may fail for a short time after its cache is disabled.
auto constexpr kDeleteBucketAttempts = 5;
auto constexpr kDeleteBucketBackoff = std::chrono::seconds(30);

// A zone where Rapid Cache Ultra is available. The buckets are created in the
// zone's region, which must match `GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID`.
// Note this is deliberately *not* `GOOGLE_CLOUD_CPP_TEST_ZONE`, which is shared
// with other libraries: creating a cache is a billable, mutating operation and
// must be enabled explicitly.
auto constexpr kZoneEnvVar = "GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_ZONE";
auto constexpr kRegionEnvVar = "GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID";

// These tests run against production by default. To run them against another
// environment, such as pre-prod, use the standard endpoint overrides:
//   GOOGLE_CLOUD_CPP_STORAGE_CONTROL_ENDPOINT=<host>:443
//   GOOGLE_CLOUD_CPP_STORAGE_CONTROL_AUTHORITY=<host>
//   GOOGLE_CLOUD_CPP_STORAGE_TEST_JSON_ENDPOINT=https://<host>
//   GOOGLE_CLOUD_CPP_STORAGE_TEST_TARGET_API_VERSION=<version>

// Every test is independent of the order in which the tests run. The tests that
// read a cache share one bucket and one cache, created once in
// `SetUpTestSuite()`, because creating a cache is slow and billable.
// `TearDownTestSuite()` disables that cache and deletes the bucket so a run
// does not leak them.
class RapidCacheIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  static void SetUpTestSuite() {
    if (UsingEmulator()) return;
    zone_name_ = google::cloud::internal::GetEnv(kZoneEnvVar).value_or("");
    region_name_ = google::cloud::internal::GetEnv(kRegionEnvVar).value_or("");
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    if (zone_name_.empty() || region_name_.empty() || project_id_.empty()) {
      return;
    }
    if (!absl::StartsWith(zone_name_, region_name_ + "-")) {
      setup_error_ = std::string(kZoneEnvVar) + "=" + zone_name_ +
                     " is not in " + kRegionEnvVar + "=" + region_name_;
      return;
    }

    client_ =
        std::make_unique<StorageControlClient>(MakeStorageControlConnection());
    RemoveStaleRapidCacheBuckets();

    auto bucket = CreateRapidCacheBucket();
    if (!bucket) {
      shared_cache_ = std::move(bucket).status();
      return;
    }
    bucket_name_ = *std::move(bucket);
    shared_cache_ = CreateCache(bucket_name_, kDefaultTtlSeconds);
  }

  static void TearDownTestSuite() {
    if (client_ != nullptr && !bucket_name_.empty()) {
      DisableCacheAndDeleteBucket(bucket_name_);
    }
    bucket_name_.clear();
    shared_cache_ = StatusOr<RapidCache>{};
    client_ = nullptr;
  }

  void SetUp() override {
    if (UsingEmulator()) {
      GTEST_SKIP() << "The storage testbench does not support Rapid Cache.";
    }
    ASSERT_TRUE(setup_error_.empty()) << setup_error_;
    if (client_ == nullptr) {
      GTEST_SKIP() << "Set " << kZoneEnvVar << ", " << kRegionEnvVar
                   << ", and GOOGLE_CLOUD_PROJECT to run the Rapid Cache"
                   << " integration tests.";
    }
  }

  static std::string Parent(std::string const& bucket) {
    return "projects/_/buckets/" + bucket;
  }

  static std::string CacheName(std::string const& bucket,
                               std::string const& zone) {
    return Parent(bucket) + "/rapidCaches/" + zone;
  }

  // Creates a regional bucket with hierarchical namespace and uniform
  // bucket-level access, as Rapid Cache requires.
  static StatusOr<std::string> CreateRapidCacheBucket() {
    auto gen = google::cloud::internal::MakeDefaultPRNG();
    auto name = testing::MakeRandomBucketName(gen, kBucketPrefix);
    auto metadata = MakeBucketIntegrationTestClient().CreateBucketForProject(
        name, project_id_,
        BucketMetadata()
            .set_location(region_name_)
            .set_hierarchical_namespace(BucketHierarchicalNamespace{true})
            .set_iam_configuration(BucketIamConfiguration{
                UniformBucketLevelAccess{true, {}}, std::nullopt}));
    if (!metadata) return std::move(metadata).status();
    return metadata->name();
  }

  static StatusOr<RapidCache> CreateCache(std::string const& bucket,
                                          std::int64_t ttl_seconds) {
    RapidCache cache;
    cache.set_name(CacheName(bucket, zone_name_));
    cache.set_zone(zone_name_);
    cache.set_cache_type(kCacheType);
    cache.mutable_ttl()->set_seconds(ttl_seconds);
    return AwaitLro(client_->CreateRapidCache(Parent(bucket), cache));
  }

  // Best-effort cleanup. Failures are logged but do not fail the suite: the
  // dedicated tests assert on `DisableRapidCache`, and buckets left behind are
  // removed by later runs.
  static void DisableCacheAndDeleteBucket(std::string const& bucket) {
    for (StatusOr<RapidCache> const& cache :
         client_->ListRapidCaches(Parent(bucket))) {
      if (!cache) {
        if (cache.status().code() == StatusCode::kNotFound) return;
        GTEST_LOG_(WARNING) << "Could not list the caches in " << bucket << ": "
                            << cache.status();
        break;
      }
      if (cache->state() == "disabled") continue;
      StatusOr<RapidCache> const disabled =
          AwaitLro(client_->DisableRapidCache(cache->name()));
      if (!disabled) {
        // The bucket cannot be deleted while it has an active cache.
        GTEST_LOG_(WARNING) << "Could not disable " << cache->name() << ": "
                            << disabled.status() << ". Leaving bucket "
                            << bucket << " for a later run to remove.";
        return;
      }
    }
    auto client = MakeBucketIntegrationTestClient();
    Status status;
    for (int attempt = 0; attempt != kDeleteBucketAttempts; ++attempt) {
      if (attempt != 0) std::this_thread::sleep_for(kDeleteBucketBackoff);
      status = testing::RemoveBucketAndContents(client, bucket);
      if (status.ok() || status.code() == StatusCode::kNotFound) return;
    }
    GTEST_LOG_(WARNING) << "Could not delete bucket " << bucket << ": "
                        << status << ". A later run will remove it.";
  }

  // Removes buckets that earlier runs created but failed to delete. The caches
  // in those buckets are disabled first, because a bucket with an active cache
  // cannot be deleted.
  static void RemoveStaleRapidCacheBuckets() {
    std::regex const re("^" + std::string(kBucketPrefix) +
                        R"re(-\d{4}-\d{2}-\d{2}_.*$)re");
    auto const created_time_limit =
        std::chrono::system_clock::now() - kStaleBucketAge;
    for (auto& bucket :
         MakeBucketIntegrationTestClient().ListBucketsForProject(project_id_)) {
      if (!bucket) return;
      if (!std::regex_match(bucket->name(), re)) continue;
      if (bucket->time_created() > created_time_limit) continue;
      DisableCacheAndDeleteBucket(bucket->name());
    }
  }

  static StatusOr<RapidCache> AwaitLro(future<StatusOr<RapidCache>> pending) {
    if (pending.wait_for(kLroTimeout) != std::future_status::ready) {
      return Status(StatusCode::kDeadlineExceeded,
                    "timed out waiting for the rapid cache operation");
    }
    return pending.get();
  }

  static std::string zone_name_;
  static std::string region_name_;
  static std::string project_id_;
  static std::string setup_error_;
  static std::string bucket_name_;
  static std::unique_ptr<StorageControlClient> client_;
  static StatusOr<RapidCache> shared_cache_;
};

std::string RapidCacheIntegrationTest::zone_name_;
std::string RapidCacheIntegrationTest::region_name_;
std::string RapidCacheIntegrationTest::project_id_;
std::string RapidCacheIntegrationTest::setup_error_;
std::string RapidCacheIntegrationTest::bucket_name_;
std::unique_ptr<StorageControlClient> RapidCacheIntegrationTest::client_;
StatusOr<RapidCache> RapidCacheIntegrationTest::shared_cache_;

// Asserts on the cache created in `SetUpTestSuite()`. This is the only test
// that reports a shared bucket or cache creation failure; the tests that read
// the cache skip instead, to avoid repeating the same diagnostic many times.
TEST_F(RapidCacheIntegrationTest, CreateRapidCache) {
  ASSERT_THAT(shared_cache_, IsOk());
  EXPECT_THAT(shared_cache_->name(), Eq(CacheName(bucket_name_, zone_name_)));
  EXPECT_THAT(shared_cache_->zone(), Eq(zone_name_));
  EXPECT_THAT(shared_cache_->cache_type(), Eq(kCacheType));
  EXPECT_THAT(shared_cache_->state(), Eq("running"));
  EXPECT_THAT(shared_cache_->ttl().seconds(), Eq(kDefaultTtlSeconds));
}

TEST_F(RapidCacheIntegrationTest, CreateRapidCacheInvalidZone) {
  if (bucket_name_.empty()) {
    GTEST_SKIP() << "no shared bucket: " << shared_cache_.status();
  }
  RapidCache cache;
  cache.set_name(CacheName(bucket_name_, "invalid-zone"));
  cache.set_zone("invalid-zone");
  cache.set_cache_type(kCacheType);

  StatusOr<RapidCache> const invalid =
      AwaitLro(client_->CreateRapidCache(Parent(bucket_name_), cache));
  EXPECT_THAT(invalid, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(RapidCacheIntegrationTest, CreateDuplicateRapidCache) {
  if (!shared_cache_) {
    GTEST_SKIP() << "no shared cache: " << shared_cache_.status();
  }
  StatusOr<RapidCache> const duplicate =
      CreateCache(bucket_name_, kDefaultTtlSeconds);
  EXPECT_THAT(duplicate, StatusIs(StatusCode::kAlreadyExists));
}

TEST_F(RapidCacheIntegrationTest, GetRapidCache) {
  if (!shared_cache_) {
    GTEST_SKIP() << "no shared cache: " << shared_cache_.status();
  }
  StatusOr<RapidCache> const cache =
      client_->GetRapidCache(CacheName(bucket_name_, zone_name_));
  ASSERT_THAT(cache, IsOk());
  EXPECT_THAT(cache->name(), Eq(CacheName(bucket_name_, zone_name_)));
  EXPECT_THAT(cache->zone(), Eq(zone_name_));
  EXPECT_THAT(cache->cache_type(), Eq(kCacheType));
}

TEST_F(RapidCacheIntegrationTest, GetNonExistentRapidCache) {
  if (bucket_name_.empty()) {
    GTEST_SKIP() << "no shared bucket: " << shared_cache_.status();
  }
  StatusOr<RapidCache> const missing =
      client_->GetRapidCache(CacheName(bucket_name_, "non-existent-12345"));
  EXPECT_THAT(missing, StatusIs(StatusCode::kNotFound));
}

TEST_F(RapidCacheIntegrationTest, ListRapidCaches) {
  if (!shared_cache_) {
    GTEST_SKIP() << "no shared cache: " << shared_cache_.status();
  }
  std::vector<std::string> names;
  for (StatusOr<RapidCache> const& cache :
       client_->ListRapidCaches(Parent(bucket_name_))) {
    ASSERT_THAT(cache, IsOk());
    names.push_back(cache->name());
  }
  EXPECT_THAT(names, Contains(CacheName(bucket_name_, zone_name_)));
}

TEST_F(RapidCacheIntegrationTest, UpdateRapidCache) {
  if (!shared_cache_) {
    GTEST_SKIP() << "no shared cache: " << shared_cache_.status();
  }
  RapidCache cache;
  cache.set_name(CacheName(bucket_name_, zone_name_));
  cache.mutable_ttl()->set_seconds(kUpdatedTtlSeconds);
  google::protobuf::FieldMask field_mask;
  field_mask.add_paths("ttl");

  StatusOr<RapidCache> const updated =
      AwaitLro(client_->UpdateRapidCache(cache, field_mask));
  ASSERT_THAT(updated, IsOk());
  EXPECT_THAT(updated->ttl().seconds(), Eq(kUpdatedTtlSeconds));
}

TEST_F(RapidCacheIntegrationTest, DisableNonExistentCache) {
  if (bucket_name_.empty()) {
    GTEST_SKIP() << "no shared bucket: " << shared_cache_.status();
  }
  StatusOr<RapidCache> const disabled = AwaitLro(client_->DisableRapidCache(
      CacheName(bucket_name_, "non-existent-12345")));
  if (disabled.status().code() == StatusCode::kUnimplemented) {
    GTEST_SKIP() << "DisableRapidCache is not yet supported by the service: "
                 << disabled.status();
  }
  EXPECT_THAT(disabled, StatusIs(StatusCode::kNotFound));
}

// Exercises the full create/disable lifecycle in a bucket of its own, so that
// it neither depends on nor disturbs the shared cache. There can be at most one
// cache per (bucket, zone), and a disabled cache is not immediately
// re-creatable.
TEST_F(RapidCacheIntegrationTest, CreateAndDisableRapidCache) {
  auto bucket = CreateRapidCacheBucket();
  ASSERT_THAT(bucket, IsOk());
  struct BucketCleanup {
    std::string name;
    ~BucketCleanup() { DisableCacheAndDeleteBucket(name); }
  } cleanup{*bucket};

  StatusOr<RapidCache> const created = CreateCache(*bucket, kDefaultTtlSeconds);
  ASSERT_THAT(created, IsOk());
  EXPECT_THAT(created->name(), Eq(CacheName(*bucket, zone_name_)));

  StatusOr<RapidCache> const disabled =
      AwaitLro(client_->DisableRapidCache(CacheName(*bucket, zone_name_)));
  if (disabled.status().code() == StatusCode::kUnimplemented) {
    GTEST_SKIP() << "DisableRapidCache is not yet supported by the service: "
                 << disabled.status();
  }
  ASSERT_THAT(disabled, IsOk());
  EXPECT_THAT(disabled->state(), Eq("disabled"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
