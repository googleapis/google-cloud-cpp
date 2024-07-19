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

#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/filesystem.h"
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/grpc_plugin.h"
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/storage/testing/remove_stale_buckets.h"
#include "google/cloud/internal/getenv.h"
#include "absl/strings/match.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
namespace {

absl::optional<std::string> EmulatorEndpoint() {
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT");
}

bool UseGrpcForMetadata() {
  auto v =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG")
          .value_or("");
  return absl::StrContains(v, "metadata");
}

bool UseGrpcForMedia() {
  auto v =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG")
          .value_or("");
  return absl::StrContains(v, "media");
}

}  // namespace

StorageIntegrationTest::~StorageIntegrationTest() {
  // The client configured to create and delete buckets is good for our
  // purposes.
  auto client = MakeBucketIntegrationTestClient();
  for (auto& o : objects_to_delete_) {
    (void)client.DeleteObject(o.bucket(), o.name(), Generation(o.generation()));
  }
  for (auto& b : buckets_to_delete_) {
    (void)RemoveBucketAndContents(client, b);
  }
}

google::cloud::storage::Client
StorageIntegrationTest::MakeIntegrationTestClient(google::cloud::Options opts) {
  opts = google::cloud::internal::MergeOptions(
      std::move(opts), Options{}
                           .set<RetryPolicyOption>(TestRetryPolicy())
                           .set<BackoffPolicyOption>(TestBackoffPolicy()));
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  if (UseGrpcForMedia() || UseGrpcForMetadata()) {
    return storage_experimental::DefaultGrpcClient(std::move(opts));
  }
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  return Client(std::move(opts));
}

google::cloud::storage::Client
StorageIntegrationTest::MakeBucketIntegrationTestClient() {
  if (UsingEmulator()) return MakeIntegrationTestClient(Options{});

  auto constexpr kInitialDelay = std::chrono::seconds(5);
  auto constexpr kMaximumBackoffDelay = std::chrono::minutes(5);
  auto constexpr kBackoffScalingFactor = 2.0;
  // This is comparable to the timeout for each integration test, it makes
  // little sense to wait any longer.
  auto constexpr kMaximumRetryTime = std::chrono::minutes(10);
  return MakeIntegrationTestClient(
      Options{}
          .set<RetryPolicyOption>(
              LimitedTimeRetryPolicy(kMaximumRetryTime).clone())
          .set<BackoffPolicyOption>(
              ExponentialBackoffPolicy(kInitialDelay, kMaximumBackoffDelay,
                                       kBackoffScalingFactor)
                  .clone()));
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
  if (UsingGrpc() && !UsingEmulator()) {
    return LimitedTimeRetryPolicy(/*maximum_duration=*/std::chrono::minutes(10))
        .clone();
  }
  return LimitedTimeRetryPolicy(/*maximum_duration=*/std::chrono::minutes(2))
      .clone();
}

std::string StorageIntegrationTest::RandomBucketNamePrefix() {
  return "gcs-grpc-team-cloud-cpp-testing";
}

std::string StorageIntegrationTest::MakeRandomBucketName() {
  return testing::MakeRandomBucketName(generator_, RandomBucketNamePrefix());
}

std::string StorageIntegrationTest::MakeRandomObjectName() {
  return "ob-" + storage::testing::MakeRandomObjectName(generator_) + ".txt";
}

std::string StorageIntegrationTest::MakeRandomFilename() {
  return google::cloud::internal::PathAppend(
      ::testing::TempDir(), storage::testing::MakeRandomFileName(generator_));
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
  auto emulator = EmulatorEndpoint();
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

StatusOr<StorageIntegrationTest::RetryTestResponse>
StorageIntegrationTest::InsertRetryTest(RetryTestRequest const& request) {
  if (!UsingEmulator()) {
    return Status(StatusCode::kUnimplemented,
                  "no retry tests without the testbench");
  }
  auto retry_client = RetryClient();

  namespace rest = ::google::cloud::rest_internal;
  auto http_request = rest::RestRequest()
                          .AddHeader("Content-Type", "application/json")
                          .SetPath("retry_test");
  auto const payload = [&request] {
    auto instructions = nlohmann::json{};
    for (auto const& i : request.instructions) {
      instructions[i.rpc_name] = i.actions;
    }
    return nlohmann::json{{"instructions", instructions}}.dump();
  }();

  auto attempts = 0;
  auto delay = std::chrono::milliseconds(250);
  auto keep_trying = [&attempts, &delay]() mutable {
    if (attempts >= 3) return false;
    if (++attempts == 1) return true;
    std::this_thread::sleep_for(delay);
    delay *= 2;
    return true;
  };

  while (keep_trying()) {
    rest_internal::RestContext context;
    auto http_response = retry_client->Post(context, http_request,
                                            {absl::Span<char const>{payload}});
    if (!http_response) continue;
    if ((*http_response)->StatusCode() != rest::HttpStatusCode::kOk) continue;
    auto response_payload =
        rest::ReadAll(std::move(**http_response).ExtractPayload());
    if (!response_payload) continue;
    auto json = nlohmann::json::parse(*response_payload);
    return RetryTestResponse{json.value("id", "")};
  }
  return Status{StatusCode::kUnavailable, "too many failures"};
}

std::shared_ptr<google::cloud::rest_internal::RestClient>
StorageIntegrationTest::RetryClient() {
  std::lock_guard<std::mutex> lk(mu_);
  if (retry_client_) return retry_client_;
  retry_client_ = google::cloud::rest_internal::MakePooledRestClient(
      EmulatorEndpoint().value(), Options{});
  return retry_client_;
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
