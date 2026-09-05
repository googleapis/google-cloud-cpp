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
#include "google/storage/v2/storage.pb.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <numeric>
#include <string>
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

struct WriteTestParams {
  LocationType location_type;
  std::size_t object_size;
  bool close_without_finalizing;
  std::size_t flush_threshold;
};

auto AlwaysRetry(Options opts) {
  return std::move(opts).set<AsyncIdempotencyPolicyOption>(
      MakeAlwaysRetryAsyncIdempotencyPolicy);
}

class RcuBidiWriteIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<WriteTestParams> {
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

  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> StartUpload(
      AsyncClient& async, BucketName const& bucket, std::string const& object) {
    auto const params = GetParam();
    if (params.location_type == LocationType::kRegionalRapid ||
        params.location_type == LocationType::kZonalRapid) {
      google::storage::v2::BidiWriteObjectRequest request;
      auto* spec = request.mutable_write_object_spec();
      spec->set_appendable(true);
      auto* resource = spec->mutable_resource();
      resource->set_name(object);
      resource->set_bucket(bucket.FullName());
      resource->set_storage_class("RAPID");
      return async.StartAppendableObjectUpload(std::move(request),
                                               AlwaysRetry(options()));
    }
    return async.StartAppendableObjectUpload(bucket, object,
                                             AlwaysRetry(options()));
  }

  // Simulated writing helper that respects the simulated flush threshold
  StatusOr<AsyncToken> WriteWithSimulatedFlush(AsyncWriter& writer,
                                               AsyncToken token,
                                               std::string const& data,
                                               std::size_t threshold) {
    if (threshold == 0 || data.size() <= threshold) {
      return writer.Write(std::move(token), WritePayload(data)).get();
    }
    std::size_t offset = 0;
    while (offset < data.size()) {
      std::size_t chunk_size = std::min(threshold, data.size() - offset);
      auto p = writer
                   .Write(std::move(token),
                          WritePayload(data.substr(offset, chunk_size)))
                   .get();
      if (!p) return p.status();
      token = *std::move(p);
      offset += chunk_size;
      if (offset < data.size()) {
        auto flush_status = writer.Flush().get();
        if (!flush_status.ok()) return flush_status;
      }
    }
    return token;
  }

 private:
  std::string bucket_name_;
  Options options_;
};

// 2.1.1 appendableUpload_emptyObject
// Only run once per location type
TEST_P(RcuBidiWriteIntegrationTest, AppendableUploadEmptyObject) {
  auto const params = GetParam();
  // Avoid duplicate runs for empty object
  if (params.object_size != 5 || params.close_without_finalizing ||
      params.flush_threshold != 0) {
    GTEST_SKIP();
  }

  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();

  auto w = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w);
  auto [writer, token] = *std::move(w);

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->size(), 0);
}

// 2.1.2 appendableUpload_bytes
TEST_P(RcuBidiWriteIntegrationTest, AppendableUploadBytes) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);

  auto w = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w);
  auto [writer, token] = *std::move(w);

  // Split into halves
  std::size_t mid = data.size() / 2;
  std::string chunk1 = data.substr(0, mid);
  std::string chunk2 = data.substr(mid);

  auto p1 = WriteWithSimulatedFlush(writer, std::move(token), chunk1,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p1);
  token = *std::move(p1);

  auto p2 = WriteWithSimulatedFlush(writer, std::move(token), chunk2,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p2);
  token = *std::move(p2);

  if (params.close_without_finalizing) {
    auto close_status = writer.Close().get();
    ASSERT_STATUS_OK(close_status);

    // Takeover to finalize
    auto client = MakeIntegrationTestClient(options());
    auto meta = client.GetObjectMetadata(bucket_name(), object_name);
    ASSERT_STATUS_OK(meta);

    auto w2 = async
                  .ResumeAppendableObjectUpload(BucketName(bucket_name()),
                                                object_name, meta->generation(),
                                                AlwaysRetry(options()))
                  .get();
    ASSERT_STATUS_OK(w2);
    auto [writer2, token2] = *std::move(w2);

    auto metadata = writer2.Finalize(std::move(token2)).get();
    ASSERT_STATUS_OK(metadata);
    ScheduleForDelete(*metadata);
    EXPECT_EQ(metadata->size(), data.size());
  } else {
    auto metadata = writer.Finalize(std::move(token)).get();
    ASSERT_STATUS_OK(metadata);
    ScheduleForDelete(*metadata);
    EXPECT_EQ(metadata->size(), data.size());
  }
}

// 2.1.3 explicitFlush
TEST_P(RcuBidiWriteIntegrationTest, ExplicitFlush) {
  auto const params = GetParam();
  // Run once to test manual flush behavior
  if (params.object_size != 500 || params.close_without_finalizing ||
      params.flush_threshold != 0) {
    GTEST_SKIP();
  }

  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string chunk1 = "Flush me, ";
  std::string chunk2 = "please.";

  auto w = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w);
  auto [writer, token] = *std::move(w);

  auto p1 = writer.Write(std::move(token), WritePayload(chunk1)).get();
  ASSERT_STATUS_OK(p1);
  token = *std::move(p1);

  auto flush_status = writer.Flush().get();
  ASSERT_STATUS_OK(flush_status);

  auto p2 = writer.Write(std::move(token), WritePayload(chunk2)).get();
  ASSERT_STATUS_OK(p2);
  token = *std::move(p2);

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->size(), chunk1.size() + chunk2.size());
}

// 2.1.4 appendableBlobUploadTakeover
TEST_P(RcuBidiWriteIntegrationTest, AppendableBlobUploadTakeover) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);

  std::size_t mid = data.size() / 2;
  std::string chunk1 = data.substr(0, mid);
  std::string chunk2 = data.substr(mid);

  auto w1 = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w1);
  auto [writer1, token1] = *std::move(w1);

  auto p1 = WriteWithSimulatedFlush(writer1, std::move(token1), chunk1,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p1);
  token1 = *std::move(p1);

  // Close session 1 without finalizing
  auto close_status = writer1.Close().get();
  ASSERT_STATUS_OK(close_status);

  // Fetch generation to resume
  auto client = MakeIntegrationTestClient(options());
  auto meta1 = client.GetObjectMetadata(bucket_name(), object_name);
  ASSERT_STATUS_OK(meta1);
  auto generation = meta1->generation();

  // Session 2 takes over
  auto w2 =
      async
          .ResumeAppendableObjectUpload(BucketName(bucket_name()), object_name,
                                        generation, AlwaysRetry(options()))
          .get();
  ASSERT_STATUS_OK(w2);
  auto [writer2, token2] = *std::move(w2);

  auto p2 = WriteWithSimulatedFlush(writer2, std::move(token2), chunk2,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p2);
  token2 = *std::move(p2);

  auto metadata = writer2.Finalize(std::move(token2)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->size(), data.size());
}

// 2.1.5 takeoverJustToFinalize
TEST_P(RcuBidiWriteIntegrationTest, TakeoverJustToFinalize) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);

  auto w1 = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w1);
  auto [writer1, token1] = *std::move(w1);

  auto p1 = WriteWithSimulatedFlush(writer1, std::move(token1), data,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p1);
  token1 = *std::move(p1);

  auto close_status = writer1.Close().get();
  ASSERT_STATUS_OK(close_status);

  auto client = MakeIntegrationTestClient(options());
  auto meta1 = client.GetObjectMetadata(bucket_name(), object_name);
  ASSERT_STATUS_OK(meta1);
  auto generation = meta1->generation();

  // Session 2 takes over just to finalize
  auto w2 =
      async
          .ResumeAppendableObjectUpload(BucketName(bucket_name()), object_name,
                                        generation, AlwaysRetry(options()))
          .get();
  ASSERT_STATUS_OK(w2);
  auto [writer2, token2] = *std::move(w2);

  auto metadata = writer2.Finalize(std::move(token2)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->size(), data.size());
}

// 2.1.6 explicitFinalizeWithCorrectChecksum
TEST_P(RcuBidiWriteIntegrationTest, ExplicitFinalizeWithCorrectChecksum) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);
  auto crc32c = ComputeCrc32cChecksum(data);

  auto w = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w);
  auto [writer, token] = *std::move(w);

  auto p = WriteWithSimulatedFlush(writer, std::move(token), data,
                                   params.flush_threshold);
  ASSERT_STATUS_OK(p);
  token = *std::move(p);

  auto opts = options();
  opts.set<PrecomputedChecksumsOption>(PrecomputedChecksums{crc32c});
  google::cloud::internal::OptionsSpan span(opts);

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);
}

// 2.1.7 explicitFinalizeWithIncorrectChecksumFails
TEST_P(RcuBidiWriteIntegrationTest,
       ExplicitFinalizeWithIncorrectChecksumFails) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);
  auto incorrect_crc32c = "incorrect_hash";

  auto w = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w);
  auto [writer, token] = *std::move(w);

  auto p = WriteWithSimulatedFlush(writer, std::move(token), data,
                                   params.flush_threshold);
  ASSERT_STATUS_OK(p);
  token = *std::move(p);

  auto opts = options();
  opts.set<PrecomputedChecksumsOption>(PrecomputedChecksums{incorrect_crc32c});
  google::cloud::internal::OptionsSpan span(opts);

  auto metadata = writer.Finalize(std::move(token)).get();
  EXPECT_THAT(metadata, Not(IsOk()));
}

// 2.1.8 takeoverJustToFinalizeWithIncorrectChecksumFails
TEST_P(RcuBidiWriteIntegrationTest,
       TakeoverJustToFinalizeWithIncorrectChecksumFails) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);
  auto incorrect_crc32c = "incorrect_hash";

  auto w1 = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w1);
  auto [writer1, token1] = *std::move(w1);

  auto p1 = WriteWithSimulatedFlush(writer1, std::move(token1), data,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p1);
  token1 = *std::move(p1);

  auto close_status = writer1.Close().get();
  ASSERT_STATUS_OK(close_status);

  auto client = MakeIntegrationTestClient(options());
  auto meta1 = client.GetObjectMetadata(bucket_name(), object_name);
  ASSERT_STATUS_OK(meta1);
  auto generation = meta1->generation();

  auto w2 =
      async
          .ResumeAppendableObjectUpload(BucketName(bucket_name()), object_name,
                                        generation, AlwaysRetry(options()))
          .get();
  ASSERT_STATUS_OK(w2);
  auto [writer2, token2] = *std::move(w2);

  auto opts = options();
  opts.set<PrecomputedChecksumsOption>(PrecomputedChecksums{incorrect_crc32c});
  google::cloud::internal::OptionsSpan span(opts);

  auto metadata = writer2.Finalize(std::move(token2)).get();
  EXPECT_THAT(metadata, Not(IsOk()));
}

// 2.1.9 takeoverAndAppendWithCorrectChecksumWorks
TEST_P(RcuBidiWriteIntegrationTest, TakeoverAndAppendWithCorrectChecksumWorks) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);

  std::size_t mid = data.size() / 2;
  std::string chunk1 = data.substr(0, mid);
  std::string chunk2 = data.substr(mid);
  auto total_crc32c = ComputeCrc32cChecksum(data);

  auto w1 = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w1);
  auto [writer1, token1] = *std::move(w1);

  auto p1 = WriteWithSimulatedFlush(writer1, std::move(token1), chunk1,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p1);
  token1 = *std::move(p1);

  auto close_status = writer1.Close().get();
  ASSERT_STATUS_OK(close_status);

  auto client = MakeIntegrationTestClient(options());
  auto meta1 = client.GetObjectMetadata(bucket_name(), object_name);
  ASSERT_STATUS_OK(meta1);
  auto generation = meta1->generation();

  auto w2 =
      async
          .ResumeAppendableObjectUpload(BucketName(bucket_name()), object_name,
                                        generation, AlwaysRetry(options()))
          .get();
  ASSERT_STATUS_OK(w2);
  auto [writer2, token2] = *std::move(w2);

  auto p2 = WriteWithSimulatedFlush(writer2, std::move(token2), chunk2,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p2);
  token2 = *std::move(p2);

  auto opts = options();
  opts.set<PrecomputedChecksumsOption>(PrecomputedChecksums{total_crc32c});
  google::cloud::internal::OptionsSpan span(opts);

  auto metadata = writer2.Finalize(std::move(token2)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);
}

// 2.1.10 takeoverAndAppendWithIncorrectChecksumFails
TEST_P(RcuBidiWriteIntegrationTest,
       TakeoverAndAppendWithIncorrectChecksumFails) {
  auto const params = GetParam();
  auto async = AsyncClient(options());
  auto object_name = MakeRandomObjectName();
  std::string data = MakeRandomData(params.object_size);

  std::size_t mid = data.size() / 2;
  std::string chunk1 = data.substr(0, mid);
  std::string chunk2 = data.substr(mid);
  auto incorrect_crc32c = "incorrect_hash";

  auto w1 = StartUpload(async, BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w1);
  auto [writer1, token1] = *std::move(w1);

  auto p1 = WriteWithSimulatedFlush(writer1, std::move(token1), chunk1,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p1);
  token1 = *std::move(p1);

  auto close_status = writer1.Close().get();
  ASSERT_STATUS_OK(close_status);

  auto client = MakeIntegrationTestClient(options());
  auto meta1 = client.GetObjectMetadata(bucket_name(), object_name);
  ASSERT_STATUS_OK(meta1);
  auto generation = meta1->generation();

  auto w2 =
      async
          .ResumeAppendableObjectUpload(BucketName(bucket_name()), object_name,
                                        generation, AlwaysRetry(options()))
          .get();
  ASSERT_STATUS_OK(w2);
  auto [writer2, token2] = *std::move(w2);

  auto p2 = WriteWithSimulatedFlush(writer2, std::move(token2), chunk2,
                                    params.flush_threshold);
  ASSERT_STATUS_OK(p2);
  token2 = *std::move(p2);

  auto opts = options();
  opts.set<PrecomputedChecksumsOption>(PrecomputedChecksums{incorrect_crc32c});
  google::cloud::internal::OptionsSpan span(opts);

  auto metadata = writer2.Finalize(std::move(token2)).get();
  EXPECT_THAT(metadata, Not(IsOk()));
}

// Generate parameter list
std::vector<WriteTestParams> GenerateParams() {
  std::vector<WriteTestParams> params;
  std::vector<LocationType> locations = {LocationType::kRegionalStandard,
                                         LocationType::kRegionalRapid,
                                         LocationType::kZonalRapid};
  std::vector<std::size_t> sizes = {5, 500, 5000};  // 5B, 500B, 5KB
  std::vector<bool> close_actions = {true, false};
  std::vector<std::size_t> flush_thresholds = {0,
                                               1024};  // no flush, flush at 1KB

  for (auto loc : locations) {
    for (auto size : sizes) {
      for (auto close : close_actions) {
        for (auto flush : flush_thresholds) {
          // If flush threshold is larger than size, it is equivalent to no
          // flush (0)
          if (flush != 0 && flush >= size) continue;
          params.push_back({loc, size, close, flush});
        }
      }
    }
  }
  return params;
}

INSTANTIATE_TEST_SUITE_P(RcuBidiWriteIntegrationTest,
                         RcuBidiWriteIntegrationTest,
                         ::testing::ValuesIn(GenerateParams()));

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
