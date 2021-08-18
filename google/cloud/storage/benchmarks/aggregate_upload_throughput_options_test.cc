// Copyright 2021 Google LLC
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

#include "google/cloud/storage/benchmarks/aggregate_upload_throughput_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

TEST(AggregateUploadThroughputOptions, Basic) {
  auto options = ParseAggregateUploadThroughputOptions(
      {
          "self-test",
          "--labels=a,b,c",
          "--bucket-name=test-bucket-name",
          "--object-prefix=test/object/prefix/",
          "--object-count=100",
          "--minimum-object-size=16MiB",
          "--maximum-object-size=32MiB",
          "--resumable-upload-chunk-size=4MiB",
          "--thread-count=42",
          "--iteration-count=10",
          "--api=XML",
          "--grpc-channel-count=16",
          "--grpc-plugin-config=default",
          "--rest-http-version=1.1",
          "--client-per-thread",
      },
      "");
  ASSERT_STATUS_OK(options);
  EXPECT_FALSE(options->exit_after_parse);
  EXPECT_EQ("a,b,c", options->labels);
  EXPECT_EQ("test-bucket-name", options->bucket_name);
  EXPECT_EQ("test/object/prefix/", options->object_prefix);
  EXPECT_EQ(100, options->object_count);
  EXPECT_EQ(16 * kMiB, options->minimum_object_size);
  EXPECT_EQ(32 * kMiB, options->maximum_object_size);
  EXPECT_EQ(42, options->thread_count);
  EXPECT_EQ(10, options->iteration_count);
  EXPECT_EQ(ApiName::kApiXml, options->api);
  EXPECT_EQ(16, options->grpc_channel_count);
  EXPECT_EQ("default", options->grpc_plugin_config);
  EXPECT_EQ("1.1", options->rest_http_version);
  EXPECT_EQ(true, options->client_per_thread);
}

TEST(AggregateUploadThroughputOptions, Description) {
  auto options = ParseAggregateUploadThroughputOptions(
      {"self-test", "--description", "fake-bucket"}, "Description for test");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(AggregateUploadThroughputOptions, Help) {
  auto options = ParseAggregateUploadThroughputOptions(
      {"self-test", "--help", "fake-bucket"}, "");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(AggregateUploadThroughputOptions, Validate) {
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions({"self-test"}, ""));
  EXPECT_FALSE(
      ParseAggregateUploadThroughputOptions({"self-test", "unused-1"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--invalid-option"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--bucket-name=b", "--thread-count=0"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--bucket-name=b", "--thread-count=-1"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--bucket-name=b", "--iteration-count=0"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--bucket-name=b", "--iteration-count=-1"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--bucket-name=b", "--api=GRPC-RAW"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--bucket-name=b", "--grpc-channel-count=-1"}, ""));
  EXPECT_FALSE(ParseAggregateUploadThroughputOptions(
      {"self-test", "--bucket-name=b", "--minimum-object-size=4MiB",
       "--maximum-object-size=2MiB"},
      ""));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
