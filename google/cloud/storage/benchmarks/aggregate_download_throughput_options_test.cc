// Copyright 2021 Google LLC
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

#include "google/cloud/storage/benchmarks/aggregate_download_throughput_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

namespace gcs = ::google::cloud::storage;
namespace gcs_ex = ::google::cloud::storage_experimental;

TEST(AggregateDownloadThroughputOptions, Basic) {
  auto options = ParseAggregateDownloadThroughputOptions(
      {
          "self-test",
          "--labels=a,b,c",
          "--bucket-name=test-bucket-name",
          "--object-prefix=test/object/prefix/",
          "--thread-count=42",
          "--iteration-count=10",
          "--repeats-per-iteration=2",
          "--read-size=4MiB",
          "--read-buffer-size=1MiB",
          "--api=JSON",
          "--client-per-thread",
          "--grpc-channel-count=16",
          "--rest-http-version=1.1",
          "--rest-endpoint=https://us-central-storage.googleapis.com",
          "--grpc-endpoint=google-c2p:///storage.googleapis.com",
          "--download-stall-timeout=10s",
          "--download-stall-minimum-rate=100KiB",
          "--grpc-background-threads=4",
          "--rest-pool-size=123",
      },
      "");
  ASSERT_STATUS_OK(options);
  EXPECT_FALSE(options->exit_after_parse);
  EXPECT_EQ("a,b,c", options->labels);
  EXPECT_EQ("test-bucket-name", options->bucket_name);
  EXPECT_EQ("test/object/prefix/", options->object_prefix);
  EXPECT_EQ(42, options->thread_count);
  EXPECT_EQ(10, options->iteration_count);
  EXPECT_EQ(2, options->repeats_per_iteration);
  EXPECT_EQ(4 * kMiB, options->read_size);
  EXPECT_EQ(1 * kMiB, options->read_buffer_size);
  EXPECT_EQ("JSON", options->api);
  EXPECT_EQ(true, options->client_per_thread);
  EXPECT_EQ(16, options->client_options.get<GrpcNumChannelsOption>());
  EXPECT_EQ("1.1", options->client_options.get<gcs_ex::HttpVersionOption>());
  EXPECT_EQ("https://us-central-storage.googleapis.com",
            options->client_options.get<gcs::RestEndpointOption>());
  EXPECT_EQ("google-c2p:///storage.googleapis.com",
            options->client_options.get<EndpointOption>());
  EXPECT_EQ(std::chrono::seconds(10),
            options->client_options.get<gcs::DownloadStallTimeoutOption>());
  EXPECT_EQ(100 * kKiB,
            options->client_options.get<gcs::DownloadStallMinimumRateOption>());
  EXPECT_EQ(4,
            options->client_options.get<GrpcBackgroundThreadPoolSizeOption>());
  EXPECT_EQ(123, options->client_options.get<gcs::ConnectionPoolSizeOption>());
}

TEST(AggregateDownloadThroughputOptions, Description) {
  auto options = ParseAggregateDownloadThroughputOptions(
      {"self-test", "--description", "fake-bucket"}, "Description for test");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(AggregateDownloadThroughputOptions, Help) {
  auto options = ParseAggregateDownloadThroughputOptions(
      {"self-test", "--help", "fake-bucket"}, "");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(AggregateDownloadThroughputOptions, Validate) {
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions({"self-test"}, ""));
  EXPECT_FALSE(
      ParseAggregateDownloadThroughputOptions({"self-test", "unused-1"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--invalid-option"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--bucket-name=b", "--thread-count=0"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--bucket-name=b", "--thread-count=-1"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--bucket-name=b", "--iteration-count=0"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--bucket-name=b", "--iteration-count=-1"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--bucket-name=b", "--repeats-per-iteration=0"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--bucket-name=b", "--repeats-per-iteration=-1"}, ""));
  EXPECT_FALSE(ParseAggregateDownloadThroughputOptions(
      {"self-test", "--bucket-name=b", "--grpc-channel-count=-1"}, ""));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
