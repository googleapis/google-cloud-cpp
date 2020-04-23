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

#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <cstdlib>
#ifdef __unix__
#include <dirent.h>
#endif  // __unix__

namespace google {
namespace cloud {
namespace storage {
namespace testing {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::internal::Sample;

StatusOr<std::size_t> GetNumEntries(std::string const& path) {
#ifdef __unix__
  DIR* dir = opendir(path.c_str());
  if (dir == nullptr) {
    return Status(StatusCode::kInternal, "Failed to open directory \"" + path +
                                             "\": " + strerror(errno));
  }
  std::size_t count = 0;
  while (readdir(dir)) {
    ++count;
  }
  closedir(dir);
  return count;
#else   // __unix__
  return Status(StatusCode::kUnimplemented,
                "Can't check #entries in " + path +
                    ", because only UNIX systems are supported");
#endif  // __unix__
}

}  // anonymous namespace

StatusOr<std::size_t> StorageIntegrationTest::GetNumOpenFiles() {
  auto res = GetNumEntries("/proc/self/fd");
  if (!res) return res;
  if (*res < 3) {
    return Status(StatusCode::kInternal,
                  "Expected at least three entries in /proc/self/fd: ., .., "
                  "and /proc/self/fd itself, found " +
                      std::to_string(*res));
  }
  return *res - 3;
}

void StorageIntegrationTest::SetUp() {
  client_ = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client_);

  project_id_ = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id_.empty());

  bucket_name_ =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  ASSERT_FALSE(bucket_name_.empty());

  test_service_account_ =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT").value_or("");
  ASSERT_FALSE(test_service_account_.empty());

  test_signing_service_account_ =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT")
          .value_or("");
  ASSERT_FALSE(test_signing_service_account_.empty());
}

StatusOr<Client> StorageIntegrationTest::MakeIntegrationTestClient() {
  return MakeIntegrationTestClient(TestRetryPolicy());
}

StatusOr<Client> StorageIntegrationTest::MakeIntegrationTestClient(
    std::unique_ptr<RetryPolicy> retry_policy) {
  auto options = ClientOptions::CreateDefaultClientOptions();
  if (!options) {
    return std::move(options).status();
  }

  auto backoff = TestBackoffPolicy();
  auto idempotency = GetEnv("CLOUD_STORAGE_IDEMPOTENCY");
  if (!idempotency || *idempotency == "always-retry") {
    return Client(*std::move(options), *retry_policy, *backoff);
  }
  if (*idempotency == "strict") {
    return Client(*std::move(options), *retry_policy, *backoff,
                  StrictIdempotencyPolicy{});
  }
  return Status(
      StatusCode::kInvalidArgument,
      "Invalid value for CLOUD_STORAGE_IDEMPOTENCY environment variable");
}

std::unique_ptr<BackoffPolicy> StorageIntegrationTest::TestBackoffPolicy() {
  std::chrono::milliseconds initial_delay(std::chrono::seconds(1));
  auto constexpr kShortDelayForTestbench = std::chrono::milliseconds(10);
  if (UsingTestbench()) {
    initial_delay = kShortDelayForTestbench;
  }

  auto constexpr kMaximumBackoffDelay = std::chrono::minutes(5);
  auto constexpr kBackoffScalingFactor = 2.0;
  return ExponentialBackoffPolicy(initial_delay, kMaximumBackoffDelay,
                                  kBackoffScalingFactor)
      .clone();
}

std::unique_ptr<RetryPolicy> StorageIntegrationTest::TestRetryPolicy() {
  return LimitedTimeRetryPolicy(/*maximum_duration=*/std::chrono::minutes(2))
      .clone();
}

std::string StorageIntegrationTest::MakeRandomBucketName() {
  // The total length of this bucket name must be <= 63 characters,
  char constexpr kPrefix[] = "gcs-cpp-test-bucket-";  // NOLINT
  auto constexpr kMaxBucketNameLength = 63;
  static_assert(kMaxBucketNameLength > sizeof(kPrefix),
                "The bucket prefix is too long");
  return storage::testing::MakeRandomBucketName(generator_, kPrefix);
}

std::string StorageIntegrationTest::MakeRandomObjectName() {
  return "ob-" + storage::testing::MakeRandomObjectName(generator_) + ".txt";
}

std::string StorageIntegrationTest::MakeRandomFilename() {
  return storage::testing::MakeRandomFileName(generator_);
}

std::string StorageIntegrationTest::MakeEntityName() {
  // We always use the viewers for the project because it is known to exist.
  return "project-viewers-" + project_id_;
}

std::string StorageIntegrationTest::LoremIpsum() {
  return R"""(Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
)""";
}

EncryptionKeyData StorageIntegrationTest::MakeEncryptionKeyData() {
  // WARNING: generator_ PRNG has not gone through a security audit.
  // It is possible that the random numbers are sufficiently predictable to
  // make them unusable for security purposes.  Application developers should
  // consult with their security team before relying on this (or any other)
  // source for encryption keys.
  // Applications should save the key in a secure location after creating
  // them. Google Cloud Storage does not save customer-supplied keys, and if
  // lost the encrypted data cannot be decrypted.
  return CreateKeyFromGenerator(generator_);
}

bool StorageIntegrationTest::UsingTestbench() const {
  return GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT").has_value();
}

void StorageIntegrationTest::WriteRandomLines(std::ostream& upload,
                                              std::ostream& local,
                                              int line_count, int line_size) {
  auto generate_random_line = [this, line_size] {
    std::string const characters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        ".,/;:'[{]}=+-_}]`~!@#$%^&*()";
    return Sample(generator_, line_size - 1, characters);
  };

  for (int line = 0; line != line_count; ++line) {
    std::string random = generate_random_line() + "\n";
    upload << random;
    local << random;
  }
}

std::string StorageIntegrationTest::MakeRandomData(std::size_t desired_size) {
  std::size_t const line_size = 128;
  auto generate_random_line = [this](std::size_t line_size) {
    std::string const characters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        ".,/;:'[{]}=+-_}]`~!@#$%^&*()";
    return Sample(generator_, static_cast<int>(line_size - 1), characters) +
           "\n";
  };

  std::string text;
  auto const line_count = desired_size / line_size;
  for (std::size_t i = 0; i != line_count; ++i) {
    text += generate_random_line(line_size);
  }
  if (text.size() < desired_size) {
    text += generate_random_line(desired_size - text.size());
  }
  return text;
}

void StorageIntegrationTestWithHmacServiceAccount::SetUp() {
  StorageIntegrationTest::SetUp();
  service_account_ =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT").value_or("");
  ASSERT_FALSE(service_account_.empty());
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
