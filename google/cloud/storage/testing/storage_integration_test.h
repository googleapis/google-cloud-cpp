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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/well_known_headers.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/testing_util/integration_test.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
/**
 * Common class for storage integration tests.
 */
class StorageIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  ~StorageIntegrationTest() override;

  /**
   * Return a client suitable for most integration tests.
   *
   * Most integration tests, particularly when running against the emulator,
   * should use short backoff and retry periods. This returns a client so
   * configured.
   */
  static google::cloud::storage::Client MakeIntegrationTestClient(
      google::cloud::Options opts = {});

  /**
   * Return a client with retry policies suitable for CreateBucket() class.
   *
   * Creating (and deleting) buckets require (specially when using production)
   * longer backoff and retry periods. A single project cannot create more than
   * one bucket every two seconds, suggesting that the default backoff should be
   * at least that long.
   */
  static google::cloud::storage::Client MakeBucketIntegrationTestClient();

  static std::unique_ptr<BackoffPolicy> TestBackoffPolicy();
  static std::unique_ptr<RetryPolicy> TestRetryPolicy();

  static std::string RandomBucketNamePrefix();
  std::string MakeRandomBucketName();
  std::string MakeRandomObjectName();
  std::string MakeRandomFilename();

  static std::string LoremIpsum();

  EncryptionKeyData MakeEncryptionKeyData();

  static constexpr int kDefaultRandomLineCount = 1000;
  static constexpr int kDefaultLineSize = 200;

  void WriteRandomLines(std::ostream& upload, std::ostream& local,
                        int line_count = kDefaultRandomLineCount,
                        int line_size = kDefaultLineSize);

  std::string MakeRandomData(std::size_t desired_size);

  static bool UsingEmulator();

  static bool UsingGrpc();

  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();

  /// Delete the given object during the test teardown.
  void ScheduleForDelete(ObjectMetadata meta) {
    std::lock_guard<std::mutex> lk(mu_);
    objects_to_delete_.push_back(std::move(meta));
  }

  /// Delete the given bucket during the test teardown.
  void ScheduleForDelete(BucketMetadata meta) {
    std::lock_guard<std::mutex> lk(mu_);
    buckets_to_delete_.push_back(std::move(meta));
  }

  /**
   * Retry test configuration for a single RPC.
   *
   * More details on RetryTestRequest.
   */
  struct RetryTestConfiguration {
    /// The name of the RPC, e.g., "storage.objects.get"
    std::string rpc_name;

    /// Actions the testbench should take in successive calls to this RPC.
    std::vector<std::string> actions;
  };

  /**
   * A retry test configuration, expressed as a series of failures per RPC.
   *
   * The storage testbench can be configured to return specific errors and
   * failures on one or more RPCs. This is used in integration tests to verify
   * the client library handles these errors correctly.
   *
   * The testbench is configured by sending this request object (marshalled as
   * a JSON object), the testbench returns a "test id".  If the client library
   * includes this "test id" in the "x-test-id" header the testbench executes
   * the actions described for each RPC.
   *
   * In simple tests one would configure some failures, say returning 429 three
   * times before succeeding, with a single RPC, say `storage.buckets.get`.
   *
   * For more complex tests, one may need to configure multiple failures for
   * different RPCs. For example, a parallel upload may involve uploading
   * multiple objects, then composing, and then deleting the components. One
   * may be interested in simulating transient failures for each of these RPCs.
   */
  struct RetryTestRequest {
    std::vector<RetryTestConfiguration> instructions;
  };

  /// The result of creating a retry test configuration.
  struct RetryTestResponse {
    std::string id;
  };

  StatusOr<RetryTestResponse> InsertRetryTest(RetryTestRequest const& request);

 private:
  std::shared_ptr<google::cloud::rest_internal::RestClient> RetryClient();

  std::mutex mu_;
  std::vector<ObjectMetadata> objects_to_delete_;
  std::vector<BucketMetadata> buckets_to_delete_;
  std::shared_ptr<google::cloud::rest_internal::RestClient> retry_client_;
};

/**
 * Count the number of *AccessControl entities with matching name and role.
 */
template <typename AccessControlResource>
typename std::vector<AccessControlResource>::difference_type
CountMatchingEntities(std::vector<AccessControlResource> const& acl,
                      AccessControlResource const& expected) {
  return std::count_if(
      acl.begin(), acl.end(), [&expected](AccessControlResource const& x) {
        return x.entity() == expected.entity() && x.role() == expected.role();
      });
}

template <typename AccessControlResource>
std::vector<std::string> AclEntityNames(
    std::vector<AccessControlResource> const& acl) {
  std::vector<std::string> names(acl.size());
  std::transform(acl.begin(), acl.end(), names.begin(),
                 [](AccessControlResource const& x) { return x.entity(); });
  return names;
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H
