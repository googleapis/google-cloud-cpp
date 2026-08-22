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

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

#include "google/cloud/storage/async/bucket_name.h"
#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;
using ::testing::Not;

enum class LocationType { kRegionalStandard, kRegionalRapid, kZonalRapid };

struct TestParams {
  LocationType location_type;
};

class RcuBidiReadIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<TestParams> {
 protected:
  void SetUp() override {
    auto const params = GetParam();

    // Configure options
    if (params.location_type == LocationType::kRegionalRapid) {
      options_ =
          Options{}
              .set<RestEndpointOption>(
                  "https://storage-preprod-test-unified.googleusercontent.com/"
                  "storage/v1_preprod/")
              .set<EndpointOption>(
                  "storage-preprod-test-grpc.googleusercontent.com:443")
              .set<ProjectIdOption>("gcs-hyd-connector-benchmarks");
      bucket_name_ = "java-storage-reg-rapid-preprod-3fe2bb58";
    } else {
      options_ = Options{};
      bucket_name_ = MakeRandomBucketName();
    }

    options_.set<storage_experimental::EnableGrpcMetricsOption>(false)
        .set<GrpcNumChannelsOption>(1);

    auto client = MakeIntegrationTestClient(options_);

    if (params.location_type == LocationType::kRegionalRapid) {
      auto metadata = client.GetBucketMetadata(bucket_name_);
      if (!metadata) {
        GTEST_SKIP() << "Static RCU bucket " << bucket_name_
                     << " not found, skipping REGIONAL_RAPID tests.";
      }
    } else {
      BucketMetadata metadata;
      metadata.set_location("us-central1");

      if (params.location_type == LocationType::kZonalRapid) {
        metadata.set_storage_class("RAPID")
            .set_hierarchical_namespace(BucketHierarchicalNamespace{true})
            .set_custom_placement_config(
                BucketCustomPlacementConfig{{"us-central1-a"}});
      }

      auto created = client.CreateBucket(bucket_name_, metadata);
      ASSERT_STATUS_OK(created);
      ScheduleForDelete(*created);
    }

    // Initialize/warm up objects if we are in REGIONAL_RAPID and not yet
    // initialized
    if (params.location_type == LocationType::kRegionalRapid &&
        !initialized_regional_rapid_) {
      initialized_regional_rapid_ = true;
      std::cout << "Pre-creating objects for read integration tests..."
                << std::endl;

      // We will use standard object names for these static tests to prevent
      // polluting the bucket
      close_test_object_ = "test-bidi-read-close";
      zero_copy_test_object_ = "test-bidi-zero-copy";
      multiple_range_test_object_ = "test-bidi-multiple-range";
      out_of_range_test_object_ = "test-bidi-out-of-range";

      auto async = AsyncClient(options_);

      // Pre-create data
      close_test_data_ = MakeRandomData(5 * 1024 * 1024);           // 5MB
      zero_copy_test_data_ = MakeRandomData(1024 * 1024);           // 1MB
      multiple_range_test_data_ = MakeRandomData(2 * 1024 * 1024);  // 2MB
      out_of_range_test_data_ = MakeRandomData(100 * 1024);         // 100KB

      // Create objects
      ASSERT_STATUS_OK(async
                           .InsertObject(BucketName(bucket_name_),
                                         close_test_object_,
                                         WritePayload(close_test_data_))
                           .get());
      ASSERT_STATUS_OK(async
                           .InsertObject(BucketName(bucket_name_),
                                         zero_copy_test_object_,
                                         WritePayload(zero_copy_test_data_))
                           .get());
      ASSERT_STATUS_OK(
          async
              .InsertObject(BucketName(bucket_name_),
                            multiple_range_test_object_,
                            WritePayload(multiple_range_test_data_))
              .get());
      ASSERT_STATUS_OK(async
                           .InsertObject(BucketName(bucket_name_),
                                         out_of_range_test_object_,
                                         WritePayload(out_of_range_test_data_))
                           .get());

      std::cout << "Regional Rapid bucket detected. Triggering Ingest-On-Read "
                   "on all objects..."
                << std::endl;
      TriggerIngestOnRead(async, close_test_object_);
      TriggerIngestOnRead(async, zero_copy_test_object_);
      TriggerIngestOnRead(async, multiple_range_test_object_);
      TriggerIngestOnRead(async, out_of_range_test_object_);

      std::cout
          << "Sleeping for 30 minutes to allow background uptiering to RZ..."
          << std::endl;
      std::this_thread::sleep_for(std::chrono::minutes(30));
      std::cout << "Wake up! Continuing with integration test execution."
                << std::endl;
    }
  }

  void TriggerIngestOnRead(AsyncClient& async, std::string const& object_name) {
    auto r = async.ReadObject(BucketName(bucket_name_), object_name).get();
    if (!r) {
      std::cout << "Warning: Ingest-on-read trigger failed for " << object_name
                << ": " << r.status() << std::endl;
      return;
    }
    AsyncReader reader;
    AsyncToken token;
    std::tie(reader, token) = *std::move(r);
    // Read only a small chunk (single-chunk read)
    auto res = reader.Read(std::move(token)).get();
    if (!res) {
      std::cout << "Warning: Ingest-on-read trigger failed during read for "
                << object_name << ": " << res.status() << std::endl;
    }
  }

  std::string const& bucket_name() const { return bucket_name_; }
  Options const& options() const { return options_; }

  using google::cloud::storage::testing::StorageIntegrationTest::
      ScheduleForDelete;
  void ScheduleForDelete(google::storage::v2::Object const& object) {
    ScheduleForDelete(storage::ObjectMetadata{}
                          .set_bucket(MakeBucketName(object.bucket())->name())
                          .set_name(object.name())
                          .set_generation(object.generation()));
  }

  // Object names and data accessible by tests
  static std::string close_test_object_;
  static std::string zero_copy_test_object_;
  static std::string multiple_range_test_object_;
  static std::string out_of_range_test_object_;

  static std::string close_test_data_;
  static std::string zero_copy_test_data_;
  static std::string multiple_range_test_data_;
  static std::string out_of_range_test_data_;

 private:
  std::string bucket_name_;
  Options options_;
  static bool initialized_regional_rapid_;
};

// Static members definition
bool RcuBidiReadIntegrationTest::initialized_regional_rapid_ = false;
std::string RcuBidiReadIntegrationTest::close_test_object_;
std::string RcuBidiReadIntegrationTest::zero_copy_test_object_;
std::string RcuBidiReadIntegrationTest::multiple_range_test_object_;
std::string RcuBidiReadIntegrationTest::out_of_range_test_object_;
std::string RcuBidiReadIntegrationTest::close_test_data_;
std::string RcuBidiReadIntegrationTest::zero_copy_test_data_;
std::string RcuBidiReadIntegrationTest::multiple_range_test_data_;
std::string RcuBidiReadIntegrationTest::out_of_range_test_data_;

auto AlwaysRetry(Options opts) {
  return std::move(opts).set<AsyncIdempotencyPolicyOption>(
      MakeAlwaysRetryAsyncIdempotencyPolicy);
}

// Helper to pre-create object for non-RCU tests
StatusOr<google::storage::v2::Object> PreCreateObject(
    AsyncClient& async, BucketName const& bucket,
    std::string const& object_name, std::string const& data) {
  return async
      .InsertObject(bucket, object_name, WritePayload(data),
                    AlwaysRetry(Options{}))
      .get();
}

// 2.2.1 readPostStreamClose
TEST_P(RcuBidiReadIntegrationTest, ReadPostStreamClose) {
  auto const params = GetParam();
  auto async = AsyncClient(options());

  std::string object_name;
  std::string data;

  if (params.location_type == LocationType::kRegionalRapid) {
    object_name = close_test_object_;
    data = close_test_data_;
  } else {
    object_name = MakeRandomObjectName();
    data = MakeRandomData(5 * 1024 * 1024);  // 5MB
    auto pre_create =
        PreCreateObject(async, BucketName(bucket_name()), object_name, data);
    ASSERT_STATUS_OK(pre_create);
    ScheduleForDelete(*pre_create);
  }

  auto r = async
               .ReadObject(BucketName(bucket_name()), object_name,
                           AlwaysRetry(options()))
               .get();
  ASSERT_STATUS_OK(r);

  AsyncReader reader;
  AsyncToken token;
  std::tie(reader, token) = *std::move(r);

  // Start an asynchronous read
  auto read_future = reader.Read(std::move(token));

  // Cancel the stream by destroying the reader
  reader = AsyncReader();

  // Resolving the future should fail
  auto res = read_future.get();
  EXPECT_THAT(res, StatusIs(StatusCode::kCancelled));
}

// 2.2.2 zeroCopyRangeReads
TEST_P(RcuBidiReadIntegrationTest, ZeroCopyRangeReads) {
  auto const params = GetParam();
  auto async = AsyncClient(options());

  std::string object_name;
  std::string data;

  if (params.location_type == LocationType::kRegionalRapid) {
    object_name = zero_copy_test_object_;
    data = zero_copy_test_data_;
  } else {
    object_name = MakeRandomObjectName();
    data = MakeRandomData(1024 * 1024);  // 1MB
    auto pre_create =
        PreCreateObject(async, BucketName(bucket_name()), object_name, data);
    ASSERT_STATUS_OK(pre_create);
    ScheduleForDelete(*pre_create);
  }

  auto descriptor_status =
      async.Open(BucketName(bucket_name()), object_name, options()).get();
  ASSERT_STATUS_OK(descriptor_status);
  auto descriptor = *std::move(descriptor_status);

  // Trigger concurrent range reads
  auto range1 = descriptor.Read(0, 1000);
  auto range2 = descriptor.Read(2000, 3000);

  auto res1 = range1.first.Read(std::move(range1.second)).get();
  ASSERT_STATUS_OK(res1);
  EXPECT_EQ(res1->first.size(), 1000);
  auto views1 = res1->first.contents();
  ASSERT_FALSE(views1.empty());
  std::string content1;
  for (auto v : views1) content1 += std::string(v);
  EXPECT_EQ(content1, data.substr(0, 1000));

  auto res2 = range2.first.Read(std::move(range2.second)).get();
  ASSERT_STATUS_OK(res2);
  EXPECT_EQ(res2->first.size(), 1000);
  auto views2 = res2->first.contents();
  ASSERT_FALSE(views2.empty());
  std::string content2;
  for (auto v : views2) content2 += std::string(v);
  EXPECT_EQ(content2, data.substr(2000, 1000));
}

// 2.2.3 multipleRangedRead
TEST_P(RcuBidiReadIntegrationTest, MultipleRangedRead) {
  auto const params = GetParam();
  auto async = AsyncClient(options());

  std::string object_name;
  std::string data;

  if (params.location_type == LocationType::kRegionalRapid) {
    object_name = multiple_range_test_object_;
    data = multiple_range_test_data_;
  } else {
    object_name = MakeRandomObjectName();
    data = MakeRandomData(2 * 1024 * 1024);  // 2MB
    auto pre_create =
        PreCreateObject(async, BucketName(bucket_name()), object_name, data);
    ASSERT_STATUS_OK(pre_create);
    ScheduleForDelete(*pre_create);
  }

  auto descriptor_status =
      async.Open(BucketName(bucket_name()), object_name, options()).get();
  ASSERT_STATUS_OK(descriptor_status);
  auto descriptor = *std::move(descriptor_status);

  auto r1 = descriptor.Read(0, 500);
  auto r2 = descriptor.Read(1000, 1500);
  auto r3 = descriptor.Read(2000, 2500);

  auto res1 = r1.first.Read(std::move(r1.second)).get();
  ASSERT_STATUS_OK(res1);
  EXPECT_EQ(res1->first.size(), 500);

  auto res2 = r2.first.Read(std::move(r2.second)).get();
  ASSERT_STATUS_OK(res2);
  EXPECT_EQ(res2->first.size(), 500);

  auto res3 = r3.first.Read(std::move(r3.second)).get();
  ASSERT_STATUS_OK(res3);
  EXPECT_EQ(res3->first.size(), 500);
}

// 2.2.4 readFromBucketThatDoesNotExistShouldRaiseStorageExceptionWith404
TEST_P(RcuBidiReadIntegrationTest, ReadFromBucketThatDoesNotExist) {
  auto async = AsyncClient(options());
  auto open_status =
      async.Open(BucketName("non-existent-bucket-12345"), "object", options())
          .get();
  EXPECT_THAT(open_status, StatusIs(StatusCode::kNotFound));
}

// 2.2.5 outOfRange
TEST_P(RcuBidiReadIntegrationTest, OutOfRange) {
  auto const params = GetParam();
  auto async = AsyncClient(options());

  std::string object_name;
  std::string data;

  if (params.location_type == LocationType::kRegionalRapid) {
    object_name = out_of_range_test_object_;
    data = out_of_range_test_data_;
  } else {
    object_name = MakeRandomObjectName();
    data = MakeRandomData(100 * 1024);  // 100KB
    auto pre_create =
        PreCreateObject(async, BucketName(bucket_name()), object_name, data);
    ASSERT_STATUS_OK(pre_create);
    ScheduleForDelete(*pre_create);
  }

  auto descriptor_status =
      async.Open(BucketName(bucket_name()), object_name, options()).get();
  ASSERT_STATUS_OK(descriptor_status);
  auto descriptor = *std::move(descriptor_status);

  // Request range beyond object size
  auto r = descriptor.Read(data.size() + 10 * 1024, data.size() + 20 * 1024);
  auto res = r.first.Read(std::move(r.second)).get();
  EXPECT_THAT(res, StatusIs(StatusCode::kOutOfRange));
}

// Instantiate tests across locations
INSTANTIATE_TEST_SUITE_P(
    RcuBidiReadIntegrationTest, RcuBidiReadIntegrationTest,
    ::testing::Values(TestParams{LocationType::kRegionalStandard},
                      TestParams{LocationType::kRegionalRapid},
                      TestParams{LocationType::kZonalRapid}));

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
