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

#include "google/cloud/bigtable/benchmarks/benchmark_options.h"
#include "google/cloud/bigtable/benchmarks/constants.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
namespace {

TEST(BenchmarkOptions, Basic) {
  auto const now = absl::FormatTime("%FT%TZ", absl::Now(), absl::UTCTimeZone());
  auto options = ParseBenchmarkOptions(
      "suffix",
      {"self-test", "--project-id=test-project", "--instance-id=test-instance",
       "--app-profile-id=test-app-profile-id", "--table-size=10000",
       "--test-duration=300s", "--use-embedded-server=true"},
      "");
  ASSERT_STATUS_OK(options);
  EXPECT_FALSE(options->exit_after_parse);
  EXPECT_EQ("test-project", options->project_id);
  EXPECT_EQ("test-instance", options->instance_id);
  EXPECT_EQ(now, options->start_time);
  EXPECT_EQ("test-app-profile-id", options->app_profile_id);
  EXPECT_EQ(10000, options->table_size);
  EXPECT_EQ(300, options->test_duration.count());
  EXPECT_EQ(10, options->parallel_requests);
}

TEST(BenchmarkOptions, Defaults) {
  auto const re = std::regex(testing::RandomTableIdRegex());
  auto options = ParseBenchmarkOptions("suffix",
                                       {
                                           "self-test",
                                           "--project-id=a",
                                           "--instance-id=b",
                                           "--app-profile-id=c",
                                       },
                                       "");
  ASSERT_STATUS_OK(options);
  EXPECT_EQ(kDefaultThreads, options->thread_count);
  EXPECT_EQ(kDefaultTableSize, options->table_size);
  EXPECT_EQ(std::chrono::seconds(kDefaultTestDuration * 60).count(),
            options->test_duration.count());
  EXPECT_EQ(false, options->use_embedded_server);
  EXPECT_EQ(10, options->parallel_requests);
  EXPECT_TRUE(std::regex_match(options->table_id, re));
  EXPECT_THAT(options->notes, ::testing::HasSubstr(bigtable::version_string()));
}

TEST(BenchmarkOptions, Description) {
  auto options = ParseBenchmarkOptions(
      "suffix", {"self-test", "--description", "other-stuff"},
      "Description for test");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(BenchmarkOptions, Help) {
  auto options = ParseBenchmarkOptions(
      "suffix", {"self-test", "--help", "other-stuff"}, "");
  EXPECT_STATUS_OK(options);
  EXPECT_TRUE(options->exit_after_parse);
}

TEST(BenchmarkOptions, Validate) {
  EXPECT_FALSE(ParseBenchmarkOptions("suffix", {"self-test"}, ""));
  EXPECT_FALSE(ParseBenchmarkOptions("suffix", {"self-test", "unused-1"}, ""));
  EXPECT_FALSE(
      ParseBenchmarkOptions("suffix",
                            {"self-test", "--project-id=a", "--instance-id=b",
                             "--app-profile-id=c", "--thread-count=0"},
                            ""));
  EXPECT_FALSE(
      ParseBenchmarkOptions("suffix",
                            {"self-test", "--project-id=a", "--instance-id=b",
                             "--app-profile-id=c", "--table-size=0"},
                            ""));
  EXPECT_FALSE(
      ParseBenchmarkOptions("suffix",
                            {"self-test", "--project-id=a", "--instance-id=b",
                             "--app-profile-id=c", "--test-duration=0"},
                            ""));
}

}  // namespace
}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
