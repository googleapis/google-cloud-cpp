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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/well_known_headers.h"
#include "google/cloud/internal/random.h"
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
  static google::cloud::StatusOr<google::cloud::storage::Client>
  MakeIntegrationTestClient();

  /**
   * Return a client with retry policies suitable for CreateBucket() class.
   *
   * Creating (and deleting) buckets require (specially when using production)
   * longer backoff and retry periods. A single project cannot create more than
   * one bucket every two seconds, suggesting that the default backoff should be
   * at least that long.
   */
  static google::cloud::StatusOr<google::cloud::storage::Client>
  MakeBucketIntegrationTestClient();

  /// Like MakeIntegrationTestClient() but with a custom retry policy
  static google::cloud::StatusOr<google::cloud::storage::Client>
  MakeIntegrationTestClient(std::unique_ptr<RetryPolicy> retry_policy);

  /// Like MakeIntegrationTestClient() but with custom retry and bucket policies
  static google::cloud::StatusOr<google::cloud::storage::Client>
  MakeIntegrationTestClient(std::unique_ptr<RetryPolicy> retry_policy,
                            std::unique_ptr<BackoffPolicy> backoff_policy);

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

  struct ApiSwitch {
    Fields for_insert;
    IfMetagenerationNotMatch for_streaming_read;
  };

  static ApiSwitch RestApiFlags(std::string const& api) {
    if (api == "XML") {
      return ApiSwitch{
          // enables XML: this filters-out all metadata fields from
          // the InsertObject() response. JSON and XML are equivalent when no
          // metadata fields are requested, and we default to XML in that case.
          Fields(""),
          // empty option has no effect, and the default is XML
          IfMetagenerationNotMatch()};
    }
    return ApiSwitch{
        // empty option has no effect, and the default is JSON since only JSON
        // can provide all metadata fields.
        Fields(),
        // disables XML (the default) as it does not support
        // metageneration-not-match
        IfMetagenerationNotMatch(0)};
  }

 private:
  std::mutex mu_;
  std::vector<ObjectMetadata> objects_to_delete_;
  std::vector<BucketMetadata> buckets_to_delete_;
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
