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

#include "google/cloud/storage/benchmarks/create_dataset_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

TEST(ThroughputOptions, Basic) {
  auto options = ParseCreateDatasetOptions({
      "self-test",
      "--bucket-name=test-bucket-name",
      "--object-prefix=test/object/prefix/",
      "--object-count=7",
      "--minimum-object-size=16KiB",
      "--maximum-object-size=32KiB",
      "--thread-count=42",
  });
  ASSERT_STATUS_OK(options);
  EXPECT_FALSE(options->exit_after_parse);
  EXPECT_EQ("test-bucket-name", options->bucket_name);
  EXPECT_EQ("test/object/prefix/", options->object_prefix);
  EXPECT_EQ(7, options->object_count);
  EXPECT_EQ(16 * kKiB, options->minimum_object_size);
  EXPECT_EQ(32 * kKiB, options->maximum_object_size);
  EXPECT_EQ(42, options->thread_count);
}

TEST(ThroughputOptions, Description) {
  auto options =
      ParseCreateDatasetOptions({"self-test", "--description", "fake-bucket"});
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(ThroughputOptions, Help) {
  auto options =
      ParseCreateDatasetOptions({"self-test", "--help", "fake-bucket"});
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(ThroughputOptions, Validate) {
  EXPECT_FALSE(ParseCreateDatasetOptions({"self-test"}));
  EXPECT_FALSE(
      ParseCreateDatasetOptions({"self-test", "unused-1", "unused-2"}));
  EXPECT_FALSE(ParseCreateDatasetOptions({
      "self-test",
      "--bucket-name=b",
      "--minimum-object-size=8",
      "--maximum-object-size=4",
  }));
  EXPECT_FALSE(ParseCreateDatasetOptions({
      "self-test",
      "--bucket-name=b",
      "--object-count=0",
  }));
  EXPECT_FALSE(ParseCreateDatasetOptions({
      "self-test",
      "--bucket-name=b",
      "--object-count=-2",
  }));
  EXPECT_FALSE(ParseCreateDatasetOptions({
      "self-test",
      "--bucket-name=b",
      "--thread-count=0",
  }));
  EXPECT_FALSE(ParseCreateDatasetOptions({
      "self-test",
      "--bucket-name=b",
      "--thread-count=-2",
  }));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
