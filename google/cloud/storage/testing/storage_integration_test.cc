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
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/grpc_plugin.h"
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace storage {
namespace testing {

static bool UseGrpcForMetadata() {
  auto v =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG")
          .value_or("");
  return v.find("metadata") != std::string::npos;
}

static bool UseGrpcForMedia() {
  auto v =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG")
          .value_or("");
  return v.find("media") != std::string::npos;
}

StorageIntegrationTest::~StorageIntegrationTest() {
  // The client configured to create and delete buckets is good for our
  // purposes.
  auto client = MakeBucketIntegrationTestClient();
  if (!client) return;
  for (auto& o : objects_to_delete_) {
    (void)client->DeleteObject(o.bucket(), o.name(),
                               Generation(o.generation()));
  }
  for (auto& b : buckets_to_delete_) {
    (void)client->DeleteBucket(b.name());
  }
}

google::cloud::StatusOr<google::cloud::storage::Client>
StorageIntegrationTest::MakeIntegrationTestClient() {
  return MakeIntegrationTestClient(TestRetryPolicy());
}

google::cloud::StatusOr<google::cloud::storage::Client>
StorageIntegrationTest::MakeBucketIntegrationTestClient() {
  if (UsingEmulator()) return MakeIntegrationTestClient();

  auto constexpr kInitialDelay = std::chrono::seconds(5);
  auto constexpr kMaximumBackoffDelay = std::chrono::minutes(5);
  auto constexpr kBackoffScalingFactor = 2.0;
  // This is comparable to the timeout for each integration test, it makes
  // little sense to wait any longer.
  auto constexpr kMaximumRetryTime = std::chrono::minutes(10);
  return MakeIntegrationTestClient(
      LimitedTimeRetryPolicy(kMaximumRetryTime).clone(),
      ExponentialBackoffPolicy(kInitialDelay, kMaximumBackoffDelay,
                               kBackoffScalingFactor)
          .clone());
}

google::cloud::StatusOr<google::cloud::storage::Client>
StorageIntegrationTest::MakeIntegrationTestClient(
    std::unique_ptr<RetryPolicy> retry_policy) {
  return MakeIntegrationTestClient(std::move(retry_policy),
                                   TestBackoffPolicy());
}

google::cloud::StatusOr<google::cloud::storage::Client>
StorageIntegrationTest::MakeIntegrationTestClient(
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  if (!client_options) return std::move(client_options).status();

  auto opts = internal::MakeOptions(*std::move(client_options))
                  .set<RetryPolicyOption>(std::move(retry_policy))
                  .set<BackoffPolicyOption>(std::move(backoff_policy));

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  if (UseGrpcForMedia() || UseGrpcForMetadata()) {
    return storage_experimental::DefaultGrpcClient(std::move(opts));
  }
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

  auto const idempotency =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_IDEMPOTENCY")
          .value_or("always-retry");
  if (idempotency == "strict") {
    opts.set<IdempotencyPolicyOption>(StrictIdempotencyPolicy{}.clone());
  } else if (idempotency != "always-retry") {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid value for CLOUD_STORAGE_IDEMPOTENCY environment variable");
  }
  auto credentials = opts.get<Oauth2CredentialsOption>();
  return Client(std::move(opts));
}

std::unique_ptr<BackoffPolicy> StorageIntegrationTest::TestBackoffPolicy() {
  std::chrono::milliseconds initial_delay(std::chrono::seconds(1));
  auto constexpr kShortDelayForEmulator = std::chrono::milliseconds(10);
  if (UsingEmulator()) {
    initial_delay = kShortDelayForEmulator;
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

std::string StorageIntegrationTest::RandomBucketNamePrefix() {
  return "cloud-cpp-testing";
}

std::string StorageIntegrationTest::MakeRandomBucketName() {
  return testing::MakeRandomBucketName(generator_, RandomBucketNamePrefix());
}

std::string StorageIntegrationTest::MakeRandomObjectName() {
  return "ob-" + storage::testing::MakeRandomObjectName(generator_) + ".txt";
}

std::string StorageIntegrationTest::MakeRandomFilename() {
  return storage::testing::MakeRandomFileName(generator_);
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

bool StorageIntegrationTest::UsingEmulator() {
  auto emulator =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT");
  if (emulator) return true;
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
      .has_value();
}

bool StorageIntegrationTest::UsingGrpc() {
  return UseGrpcForMedia() || UseGrpcForMetadata();
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
