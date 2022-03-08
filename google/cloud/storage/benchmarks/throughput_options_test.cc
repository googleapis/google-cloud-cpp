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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(ThroughputOptions, Basic) {
  auto options = ParseThroughputOptions({
      "self-test",
      "--project-id=test-project",
      "--region=test-region",
      "--thread-count=42",
      "--minimum-object-size=16KiB",
      "--maximum-object-size=32KiB",
      "--minimum-write-size=16KiB",
      "--maximum-write-size=128KiB",
      "--write-quantum=16KiB",
      "--minimum-read-size=32KiB",
      "--maximum-read-size=256KiB",
      "--read-quantum=32KiB",
      "--duration=1s",
      "--minimum-sample-count=1",
      "--maximum-sample-count=2",
      "--enabled-libs=Raw,CppClient",
      "--enabled-transports=DirectPath,Grpc,Json,Xml",
      "--enabled-crc32c=enabled",
      "--enabled-md5=disabled",
      "--client-per-thread=false",
      "--rest-endpoint=test-only-rest",
      "--grpc-endpoint=test-only-grpc",
      "--direct-path-endpoint=test-only-direct-path",
  });
  ASSERT_STATUS_OK(options);
  EXPECT_EQ("test-project", options->project_id);
  EXPECT_EQ("test-region", options->region);
  EXPECT_EQ(42, options->thread_count);
  EXPECT_EQ(16 * kKiB, options->minimum_object_size);
  EXPECT_EQ(32 * kKiB, options->maximum_object_size);
  EXPECT_EQ(16 * kKiB, options->minimum_write_size);
  EXPECT_EQ(128 * kKiB, options->maximum_write_size);
  EXPECT_EQ(16 * kKiB, options->write_quantum);
  EXPECT_EQ(32 * kKiB, options->minimum_read_size);
  EXPECT_EQ(256 * kKiB, options->maximum_read_size);
  EXPECT_EQ(32 * kKiB, options->read_quantum);
  EXPECT_EQ(1, options->duration.count());
  EXPECT_EQ(1, options->minimum_sample_count);
  EXPECT_EQ(2, options->maximum_sample_count);
  EXPECT_THAT(options->libs,
              UnorderedElementsAre(ExperimentLibrary::kRaw,
                                   ExperimentLibrary::kCppClient));
  EXPECT_THAT(options->transports,
              UnorderedElementsAre(
                  ExperimentTransport::kDirectPath, ExperimentTransport::kGrpc,
                  ExperimentTransport::kJson, ExperimentTransport::kXml));
  EXPECT_THAT(options->enabled_crc32c, ElementsAre(true));
  EXPECT_THAT(options->enabled_md5, ElementsAre(false));
  EXPECT_EQ("test-only-rest", options->rest_endpoint);
  EXPECT_EQ("test-only-grpc", options->grpc_endpoint);
  EXPECT_EQ("test-only-direct-path", options->direct_path_endpoint);
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
      "--minimum-write-size=8",
      "--maximum-write-size=4",
  }));
  EXPECT_FALSE(ParseThroughputOptions({
      "self-test",
      "--region=r",
      "--minimum-write-size=4",
      "--maximum-write-size=8",
      "--write-quantum=5",
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
      "--read-quantum=5",
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
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
