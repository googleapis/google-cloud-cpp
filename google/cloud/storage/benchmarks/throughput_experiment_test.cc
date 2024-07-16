// Copyright 2020 Google LLC
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

#include "google/cloud/storage/benchmarks/throughput_experiment.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;

struct TestParam {
  ExperimentLibrary library;
  ExperimentTransport transport;
};

class ThroughputExperimentIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<TestParam> {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string bucket_name_;
};

bool ProductionOnly(TestParam c) {
  return c.library == ExperimentLibrary::kRaw &&
         (c.transport == ExperimentTransport::kGrpc ||
          c.transport == ExperimentTransport::kDirectPath);
}

TEST_P(ThroughputExperimentIntegrationTest, Upload) {
  if (UsingEmulator() && ProductionOnly(GetParam())) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();

  ThroughputOptions options;
  options.minimum_write_buffer_size = 1 * kMiB;
  options.libs = {GetParam().library};
  options.transports = {GetParam().transport};

  auto provider = [&](ExperimentTransport) { return client; };
  auto experiments = CreateUploadExperiments(options, provider);
  for (auto& e : experiments) {
    auto object_name = MakeRandomObjectName();
    ThroughputExperimentConfig config{
        OpType::kOpInsert,       16 * kKiB,    1 * kMiB,
        /*enable_crc32c=*/false,
        /*enable_md5=*/false,    absl::nullopt};
    auto result = e->Run(bucket_name_, object_name, config);
    ASSERT_STATUS_OK(result.status);
    auto status = client.DeleteObject(bucket_name_, object_name);
    EXPECT_THAT(status,
                StatusIs(AnyOf(StatusCode::kOk, StatusCode::kNotFound)));
  }
}

TEST_P(ThroughputExperimentIntegrationTest, Download) {
  if (UsingEmulator() && ProductionOnly(GetParam())) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();

  ThroughputOptions options;
  options.minimum_write_buffer_size = 1 * kMiB;
  options.libs = {GetParam().library};
  options.transports = {GetParam().transport};

  auto provider = [&](ExperimentTransport) { return client; };
  auto experiments =
      CreateDownloadExperiments(options, provider, /*thread_id=*/0);
  for (auto& e : experiments) {
    auto object_name = MakeRandomObjectName();

    auto constexpr kObjectSize = 16 * kKiB;
    ThroughputExperimentConfig config{OpType::kOpRead0,
                                      kObjectSize,
                                      1 * kMiB,
                                      /*enable_crc32c=*/false,
                                      /*enable_md5=*/false,
                                      std::make_pair(128 * kKiB, 256 * kKiB)};

    auto contents = MakeRandomData(kObjectSize);
    auto insert =
        client.InsertObject(bucket_name_, object_name, std::move(contents));
    ASSERT_STATUS_OK(insert);

    // With the raw protocols this might fail, that is fine, we just want the
    // code to not crash and return the result (including failures).
    (void)e->Run(bucket_name_, object_name, config);

    auto status = client.DeleteObject(bucket_name_, object_name);
    EXPECT_THAT(status,
                StatusIs(AnyOf(StatusCode::kOk, StatusCode::kNotFound)));
  }
}

INSTANTIATE_TEST_SUITE_P(ThroughputExperimentIntegrationTestJson,
                         ThroughputExperimentIntegrationTest,
                         ::testing::Values(TestParam{
                             ExperimentLibrary::kCppClient,
                             ExperimentTransport::kJson}));
INSTANTIATE_TEST_SUITE_P(ThroughputExperimentIntegrationTestGrpc,
                         ThroughputExperimentIntegrationTest,
                         ::testing::Values(TestParam{
                             ExperimentLibrary::kCppClient,
                             ExperimentTransport::kGrpc}));
INSTANTIATE_TEST_SUITE_P(ThroughputExperimentIntegrationTestRawJson,
                         ThroughputExperimentIntegrationTest,
                         ::testing::Values(TestParam{
                             ExperimentLibrary::kRaw,
                             ExperimentTransport::kJson}));
INSTANTIATE_TEST_SUITE_P(ThroughputExperimentIntegrationTestRawGrpc,
                         ThroughputExperimentIntegrationTest,
                         ::testing::Values(TestParam{
                             ExperimentLibrary::kRaw,
                             ExperimentTransport::kGrpc}));

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
