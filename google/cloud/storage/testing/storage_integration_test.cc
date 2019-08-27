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
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace storage {
namespace testing {

google::cloud::StatusOr<google::cloud::storage::Client>
StorageIntegrationTest::MakeIntegrationTestClient() {
  auto options = ClientOptions::CreateDefaultClientOptions();
  if (!options) {
    return std::move(options).status();
  }

  std::chrono::milliseconds initial_delay(std::chrono::seconds(1));
  if (UsingTestbench()) {
    initial_delay = std::chrono::milliseconds(10);
  }

  ExponentialBackoffPolicy backoff(initial_delay, std::chrono::minutes(5), 2.0);

  auto idempotency =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_IDEMPOTENCY");
  if (!idempotency || *idempotency == "always-retry") {
    return Client(*std::move(options), std::move(backoff));
  }
  if (*idempotency == "strict") {
    return Client(*std::move(options), std::move(backoff),
                  StrictIdempotencyPolicy{});
  }
  return Status(
      StatusCode::kInvalidArgument,
      "Invalid value for CLOUD_STORAGE_IDEMPOTENCY environment variable");
}

std::string StorageIntegrationTest::MakeRandomObjectName() {
  return "ob-" +
         google::cloud::internal::Sample(generator_, 16,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "012456789") +
         ".txt";
}

std::string StorageIntegrationTest::LoremIpsum() const {
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

std::string StorageIntegrationTest::MakeRandomBucketName() {
  // The total length of this bucket name must be <= 63 characters,
  static std::string const prefix = "gcs-cpp-test-bucket-";
  static std::size_t const kMaxBucketNameLength = 63;
  std::size_t const max_random_characters =
      kMaxBucketNameLength - prefix.size();
  return prefix + google::cloud::internal::Sample(
                      generator_, static_cast<int>(max_random_characters),
                      "abcdefghijklmnopqrstuvwxyz012456789");
}

bool StorageIntegrationTest::UsingTestbench() const {
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
      .has_value();
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
    return google::cloud::internal::Sample(generator_, line_size - 1,
                                           characters);
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
    return google::cloud::internal::Sample(
               generator_, static_cast<int>(line_size - 1), characters) +
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

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
