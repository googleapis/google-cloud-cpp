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

#include "google/cloud/bigtable/benchmarks/mutation_batcher_throughput_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
namespace {

TEST(MutationBatcherThroughputOptions, Basic) {
  auto options = ParseMutationBatcherThroughputOptions(
      {
          "self-test",
          "--project-id=test-project",
          "--instance-id=test-instance",
          "--table-id=test-table",
          "--max-time=200s",
          "--shard-count=2",
          "--write-thread-count=3",
          "--batcher-thread-count=4",
          "--mutation-count=2000000",
          "--max-batches=20",
          "--batch-size=2000",
      },
      "");
  ASSERT_STATUS_OK(options);
  EXPECT_FALSE(options->exit_after_parse);
  EXPECT_EQ("test-project", options->project_id);
  EXPECT_EQ("test-instance", options->instance_id);
  EXPECT_EQ("test-table", options->table_id);
  EXPECT_EQ(200, options->max_time.count());
  EXPECT_EQ(2, options->shard_count);
  EXPECT_EQ(3, options->write_thread_count);
  EXPECT_EQ(4, options->batcher_thread_count);
  EXPECT_EQ(2000000, options->mutation_count);
  EXPECT_EQ(20, options->max_batches);
  EXPECT_EQ(2000, options->batch_size);
}

TEST(MutationBatcherThroughputOptions, Defaults) {
  auto options = ParseMutationBatcherThroughputOptions(
      {
          "self-test",
          "--project-id=a",
          "--instance-id=b",
      },
      "");
  EXPECT_TRUE(options->table_id.empty());
  EXPECT_EQ(0, options->max_time.count());
  EXPECT_EQ(1, options->shard_count);
  EXPECT_EQ(1, options->write_thread_count);
  EXPECT_EQ(1, options->batcher_thread_count);
  EXPECT_EQ(1000000, options->mutation_count);
  EXPECT_EQ(10, options->max_batches);
  EXPECT_EQ(1000, options->batch_size);
}

TEST(MutationBatcherThroughputOptions, Description) {
  auto options = ParseMutationBatcherThroughputOptions(
      {"self-test", "--description", "other-stuff"}, "Description for test");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(MutationBatcherThroughputOptions, Help) {
  auto options = ParseMutationBatcherThroughputOptions(
      {"self-test", "--help", "other-stuff"}, "");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(MutationBatcherThroughputOptions, Validate) {
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions({"self-test"}, ""));
  EXPECT_FALSE(
      ParseMutationBatcherThroughputOptions({"self-test", "unused-1"}, ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--invalid-option"}, ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--instance-id=b"}, ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a"}, ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--max-time=-1s"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--shard-count=-1"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--shard-count=0"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b",
       "--write-thread-count=-1"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b",
       "--write-thread-count=0"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b",
       "--batcher-thread-count=-1"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b",
       "--batcher-thread-count=0"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--mutation_count=-1"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--max-batches=-1"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--max-batches=0"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--batch-size=-1"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--batch-size=0"},
      ""));
  EXPECT_FALSE(ParseMutationBatcherThroughputOptions(
      {"self-test", "--project-id=a", "--instance-id=b", "--batch-size=100001"},
      ""));
}

}  // namespace
}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
