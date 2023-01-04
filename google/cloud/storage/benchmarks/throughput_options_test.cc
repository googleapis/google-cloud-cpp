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

#include "google/cloud/storage/benchmarks/throughput_options.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

namespace gcs = ::google::cloud::storage;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(ThroughputOptions, Basic) {
  auto options = ParseThroughputOptions({
      "self-test",
      "--project-id=test-project",
      "--region=test-region",
      "--bucket-prefix=custom-prefix",
      "--thread-count=42",
      "--grpc-channel-count=8",
      "--direct-path-channel-count=2",
      "--minimum-object-size=16KiB",
      "--maximum-object-size=32KiB",
      "--minimum-write-buffer-size=16KiB",
      "--maximum-write-buffer-size=128KiB",
      "--write-buffer-quantum=16KiB",
      "--minimum-read-buffer-size=32KiB",
      "--maximum-read-buffer-size=256KiB",
      "--read-buffer-quantum=32KiB",
      "--duration=1s",
      "--minimum-sample-count=1",
      "--maximum-sample-count=2",
      "--enabled-libs=Raw,CppClient",
      "--enabled-transports=DirectPath,Grpc,Json",
      "--upload-functions=WriteObject",
      "--enabled-crc32c=enabled",
      "--enabled-md5=disabled",
      "--client-per-thread=false",
      "--rest-endpoint=test-only-rest",
      "--grpc-endpoint=test-only-grpc",
      "--direct-path-endpoint=test-only-direct-path",
      "--transfer-stall-timeout=86400s",
      "--transfer-stall-minimum-rate=7KiB",
      "--download-stall-timeout=86401s",
      "--download-stall-minimum-rate=9KiB",
      "--minimum-sample-delay=250ms",
      "--minimum-read-offset=32KiB",
      "--maximum-read-offset=48KiB",
      "--read-offset-quantum=8KiB",
      "--minimum-read-size=48KiB",
      "--maximum-read-size=64KiB",
      "--read-size-quantum=16KiB",
      "--target-api-version-path=vN",
      "--grpc-background-threads=16",
      "--enable-retry-loop=false",
      "--rest-pool-size=123",
      "--labels=job:foo,task:bar",
  });
  ASSERT_STATUS_OK(options);
  EXPECT_EQ("test-project", options->project_id);
  EXPECT_EQ("test-region", options->region);
  EXPECT_EQ("custom-prefix", options->bucket_prefix);
  EXPECT_EQ(42, options->thread_count);
  EXPECT_EQ(16 * kKiB, options->minimum_object_size);
  EXPECT_EQ(32 * kKiB, options->maximum_object_size);
  EXPECT_EQ(16 * kKiB, options->minimum_write_buffer_size);
  EXPECT_EQ(128 * kKiB, options->maximum_write_buffer_size);
  EXPECT_EQ(16 * kKiB, options->write_buffer_quantum);
  EXPECT_EQ(32 * kKiB, options->minimum_read_buffer_size);
  EXPECT_EQ(256 * kKiB, options->maximum_read_buffer_size);
  EXPECT_EQ(32 * kKiB, options->read_buffer_quantum);
  EXPECT_EQ(1, options->duration.count());
  EXPECT_EQ(1, options->minimum_sample_count);
  EXPECT_EQ(2, options->maximum_sample_count);
  EXPECT_THAT(options->libs,
              UnorderedElementsAre(ExperimentLibrary::kRaw,
                                   ExperimentLibrary::kCppClient));
  EXPECT_THAT(options->transports,
              UnorderedElementsAre(ExperimentTransport::kDirectPath,
                                   ExperimentTransport::kGrpc,
                                   ExperimentTransport::kJson));
  EXPECT_THAT(options->upload_functions, UnorderedElementsAre("WriteObject"));
  EXPECT_THAT(options->enabled_crc32c, ElementsAre(true));
  EXPECT_THAT(options->enabled_md5, ElementsAre(false));
  EXPECT_EQ(std::chrono::milliseconds(250), options->minimum_sample_delay);
  EXPECT_EQ(8, options->grpc_options.get<GrpcNumChannelsOption>());
  EXPECT_EQ(2, options->direct_path_options.get<GrpcNumChannelsOption>());
  EXPECT_EQ("test-only-rest",
            options->rest_options.get<gcs::RestEndpointOption>());
  EXPECT_EQ("test-only-grpc", options->grpc_options.get<EndpointOption>());
  EXPECT_EQ("test-only-direct-path",
            options->direct_path_options.get<EndpointOption>());
  EXPECT_EQ(std::chrono::seconds(86400),
            options->client_options.get<gcs::TransferStallTimeoutOption>());
  EXPECT_EQ(7 * kKiB,
            options->client_options.get<gcs::TransferStallMinimumRateOption>());
  EXPECT_EQ(std::chrono::seconds(86401),
            options->client_options.get<gcs::DownloadStallTimeoutOption>());
  EXPECT_EQ(9 * kKiB,
            options->client_options.get<gcs::DownloadStallMinimumRateOption>());
  EXPECT_EQ("vN",
            options->rest_options.get<gcs::internal::TargetApiVersionOption>());
  EXPECT_EQ(16,
            options->grpc_options.get<GrpcBackgroundThreadPoolSizeOption>());
  EXPECT_TRUE(options->client_options.has<gcs::RetryPolicyOption>());
  EXPECT_EQ(123, options->client_options.get<gcs::ConnectionPoolSizeOption>());
  EXPECT_EQ("job:foo,task:bar", options->labels);
}

TEST(ThroughputOptions, Description) {
  EXPECT_STATUS_OK(ParseThroughputOptions(
      {"self-test", "--help", "--description", "fake-region"}));
}

TEST(ThroughputOptions, ParseCrc32c) {
  EXPECT_FALSE(
      ParseThroughputOptions({"self-test", "--region=r", "--enabled-crc32c="}));
  auto options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-crc32c=enabled"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->enabled_crc32c, ElementsAre(true));
  options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-crc32c=disabled"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->enabled_crc32c, ElementsAre(false));
  options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-crc32c=random"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->enabled_crc32c, UnorderedElementsAre(false, true));
}

TEST(ThroughputOptions, MD5) {
  EXPECT_FALSE(
      ParseThroughputOptions({"self-test", "--region=r", "--enabled-md5="}));
  auto options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-md5=enabled"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->enabled_md5, ElementsAre(true));
  options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-md5=disabled"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->enabled_md5, ElementsAre(false));
  options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-md5=random"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->enabled_md5, UnorderedElementsAre(false, true));
}

TEST(ThroughputOptions, Libraries) {
  EXPECT_FALSE(
      ParseThroughputOptions({"self-test", "--region=r", "--enabled-libs="}));
  EXPECT_FALSE(ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-libs=CppClient,Raw,INVALID"}));
  auto options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-libs=CppClient,Raw"});
  EXPECT_THAT(options->libs, UnorderedElementsAre(ExperimentLibrary::kCppClient,
                                                  ExperimentLibrary::kRaw));
}

TEST(ThroughputOptions, Transports) {
  EXPECT_FALSE(ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-transports="}));
  EXPECT_FALSE(ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-transports=Grpc,INVALID"}));
  auto options = ParseThroughputOptions(
      {"self-test", "--region=r", "--enabled-transports=Grpc,Json,DirectPath"});
  EXPECT_THAT(options->transports,
              UnorderedElementsAre(ExperimentTransport::kGrpc,
                                   ExperimentTransport::kDirectPath,
                                   ExperimentTransport::kJson));
}

TEST(ThroughputOptions, UploadFunctions) {
  EXPECT_FALSE(ParseThroughputOptions(
      {"self-test", "--region=r", "--upload-functions="}));
  EXPECT_FALSE(ParseThroughputOptions(
      {"self-test", "--region=r", "--upload-functions=InsertObject,Invalid"}));
  auto options = ParseThroughputOptions(
      {"self-test", "--region=r", "--upload-functions=InsertObject"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->upload_functions, UnorderedElementsAre("InsertObject"));
  options = ParseThroughputOptions(
      {"self-test", "--region=r", "--upload-functions=WriteObject"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->upload_functions, UnorderedElementsAre("WriteObject"));
  options =
      ParseThroughputOptions({"self-test", "--region=r",
                              "--upload-functions=WriteObject,InsertObject"});
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(options->upload_functions,
              UnorderedElementsAre("InsertObject", "WriteObject"));
}

TEST(ThroughputOptions, Readoffset) {
  auto options = ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-offset=0",
      "--maximum-read-offset=2MiB",
      "--read-offset-quantum=128KiB",
  });
  ASSERT_STATUS_OK(options);
  EXPECT_EQ(options->minimum_read_offset, 0);
  EXPECT_EQ(options->maximum_read_offset, 2 * kMiB);
  EXPECT_EQ(options->read_offset_quantum, 128 * kKiB);
}

TEST(ThroughputOptions, ReadSize) {
  auto options = ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-size=0",
      "--maximum-read-size=2MiB",
      "--read-size-quantum=128KiB",
  });
  ASSERT_STATUS_OK(options);
  EXPECT_EQ(options->minimum_read_size, 0);
  EXPECT_EQ(options->maximum_read_size, 2 * kMiB);
  EXPECT_EQ(options->read_size_quantum, 128 * kKiB);
}

TEST(ThroughputOptions, Validate) {
  EXPECT_FALSE(ParseThroughputOptions({"self-test"}));
  EXPECT_FALSE(ParseThroughputOptions({"self-test", "unused-1", "unused-2"}));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-object-size=8",
      "--maximum-object-size=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--grpc-channel-count=-1",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--direct-path-channel-count=-1",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-write-buffer-size=8",
      "--maximum-write-buffer-size=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-write-buffer-size=4",
      "--maximum-write-buffer-size=8",
      "--write-buffer-quantum=5",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-buffer-size=8",
      "--maximum-read-buffer-size=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-buffer-size=4",
      "--maximum-read-buffer-size=8",
      "--read-buffer-quantum=5",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-sample-count=8",
      "--maximum-sample-count=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--thread-count=0",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--thread-count=-2",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-sample-delay=-2ms",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-sample-delay=-1ms",
  }));

  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-offset=8",
      "--maximum-read-offset=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-offset=4",
      "--maximum-read-offset=8",
      "--read-offset-quantum=5",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-offset=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--maximum-read-offset=4",
  }));

  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-size=8",
      "--maximum-read-size=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-size=4",
      "--maximum-read-size=8",
      "--read-size-quantum=5",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-read-size=8",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--maximum-read-size=8",
  }));

  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--grpc-background-threads=0",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--grpc-background-threads=-1",
  }));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
