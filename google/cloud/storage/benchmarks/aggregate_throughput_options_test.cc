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

#include "google/cloud/storage/benchmarks/aggregate_throughput_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

TEST(AggregateThroughputOptions, Basic) {
  auto options = ParseAggregateThroughputOptions(
      {
          "self-test",
          "--bucket-name=test-bucket-name",
          "--object-prefix=test/object/prefix/",
          "--thread-count=42",
          "--reporting-interval=2m",
          "--running-time=1h",
          "--read-size=4MiB",
          "--read-buffer-size=1MiB",
          "--api=XML",
          "--grpc-channel-count=16",
      },
      "");
  ASSERT_STATUS_OK(options);
  EXPECT_FALSE(options->exit_after_parse);
  EXPECT_EQ("test-bucket-name", options->bucket_name);
  EXPECT_EQ("test/object/prefix/", options->object_prefix);
  EXPECT_EQ(42, options->thread_count);
  EXPECT_EQ(std::chrono::minutes(2), options->reporting_interval);
  EXPECT_EQ(std::chrono::hours(1), options->running_time);
  EXPECT_EQ(4 * kMiB, options->read_size);
  EXPECT_EQ(1 * kMiB, options->read_buffer_size);
  EXPECT_EQ(ApiName::kApiXml, options->api);
  EXPECT_EQ(16, options->grpc_channel_count);
}

TEST(AggregateThroughputOptions, Description) {
  auto options = ParseAggregateThroughputOptions(
      {"self-test", "--description", "fake-bucket"}, "Description for test");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(AggregateThroughputOptions, Help) {
  auto options = ParseAggregateThroughputOptions(
      {"self-test", "--help", "fake-bucket"}, "");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(AggregateThroughputOptions, Validate) {
  EXPECT_FALSE(ParseAggregateThroughputOptions({"self-test"}, ""));
  EXPECT_FALSE(ParseAggregateThroughputOptions({"self-test", "unused-1"}, ""));
  EXPECT_FALSE(
      ParseAggregateThroughputOptions({"self-test", "--invalid-option"}, ""));
  EXPECT_FALSE(ParseAggregateThroughputOptions(
      {"self-test", "--bucket-name=b", "--thread-count=0"}, ""));
  EXPECT_FALSE(ParseAggregateThroughputOptions(
      {"self-test", "--bucket-name=b", "--thread-count=-1"}, ""));
  EXPECT_FALSE(ParseAggregateThroughputOptions(
      {"self-test", "--bucket-name=b", "--api=GRPC-RAW"}, ""));
  EXPECT_FALSE(ParseAggregateThroughputOptions(
      {"self-test", "--bucket-name=b", "--grpc-channel-count=-1"}, ""));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
