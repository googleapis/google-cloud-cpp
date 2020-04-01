// Copyright 2020 Google LLC
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

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <stdexcept>

namespace google {
namespace cloud {
namespace bigtable {
namespace examples {

using ::testing::HasSubstr;

TEST(BigtableExamplesCommon, Simple) {
  int test_calls = 0;
  Example example({
      {"test",
       [&](std::vector<std::string> const& args) {
         ++test_calls;
         if (args.empty()) throw Usage("test-usage");
         ASSERT_EQ(2, args.size());
         EXPECT_EQ("a0", args[0]);
         EXPECT_EQ("a1", args[1]);
       }},
  });
  char const* argv[] = {"argv0", "test", "a0", "a1"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 0);
  EXPECT_EQ(2, test_calls);
}

TEST(BigtableExamplesCommon, AutoRunDisabled) {
  google::cloud::testing_util::ScopedEnvironment env(
      "GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES", "no");
  int test_calls = 0;
  Example example({
      {"test", [&](std::vector<std::string> const&) { ++test_calls; }},
  });
  char const* argv[] = {"argv0"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 1);
  EXPECT_EQ(1, test_calls);
}

TEST(BigtableExamplesCommon, AutoRunMissing) {
  google::cloud::testing_util::ScopedEnvironment env(
      "GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES", "yes");
  int test_calls = 0;
  Example example({
      {"test", [&](std::vector<std::string> const&) { ++test_calls; }},
  });
  char const* argv[] = {"argv0"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 1);
  EXPECT_EQ(1, test_calls);
}

TEST(BigtableExamplesCommon, AutoRun) {
  google::cloud::testing_util::ScopedEnvironment env(
      "GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES", "yes");
  int test_calls = 0;
  int auto_calls = 0;
  Example example({
      {"test", [&](std::vector<std::string> const&) { ++test_calls; }},
      {"auto", [&](std::vector<std::string> const&) { ++auto_calls; }},
  });
  char const* argv[] = {"argv0"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 0);
  EXPECT_EQ(1, test_calls);
  EXPECT_EQ(1, auto_calls);
}

TEST(BigtableExamplesCommon, CommandNotFound) {
  int test_calls = 0;
  Example example({
      {"test", [&](std::vector<std::string> const&) { ++test_calls; }},
  });
  char const* argv[] = {"argv0", "wrong-name"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 1);
  EXPECT_EQ(1, test_calls);
}

TEST(BigtableExamplesCommon, CommandUsage) {
  int test_calls = 0;
  Example example({
      {"test",
       [&](std::vector<std::string> const& args) {
         ++test_calls;
         if (args.empty()) throw Usage("test-usage");
       }},
  });
  char const* argv[] = {"argv0", "test"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 1);
  EXPECT_EQ(2, test_calls);
}

TEST(BigtableExamplesCommon, CommandError) {
  int test_calls = 0;
  Example example({
      {"test",
       [&](std::vector<std::string> const& args) {
         ++test_calls;
         if (args.empty()) throw Usage("test-usage");
         throw std::runtime_error("some problem");
       }},
  });
  char const* argv[] = {"argv0", "test", "a0"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 1);
  EXPECT_EQ(2, test_calls);
}

TEST(BigtableExamplesCommon, CheckEnvironmentVariablesNormal) {
  google::cloud::testing_util::ScopedEnvironment test_a("TEST_A", "a");
  google::cloud::testing_util::ScopedEnvironment test_b("TEST_B", "b");
  EXPECT_NO_THROW(CheckEnvironmentVariablesAreSet({"TEST_A", "TEST_B"}));
}

TEST(BigtableExamplesCommon, CheckEnvironmentVariablesNotSet) {
  google::cloud::testing_util::ScopedEnvironment test_a("TEST_A", {});
  EXPECT_THROW(
      try {
        CheckEnvironmentVariablesAreSet({"TEST_A"});
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("TEST_A"));
        throw;
      },
      std::runtime_error);
}

TEST(BigtableExamplesCommon, CheckEnvironmentVariablesSetEmpty) {
  google::cloud::testing_util::ScopedEnvironment test_a("TEST_A", "");
  EXPECT_THROW(
      try {
        CheckEnvironmentVariablesAreSet({"TEST_A"});
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("TEST_A"));
        throw;
      },
      std::runtime_error);
}

TEST(BigtableExamplesCommon, RunAdminIntegrationTestsEmulator) {
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");
  google::cloud::testing_util::ScopedEnvironment admin(
      "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS", "no");
  EXPECT_TRUE(RunAdminIntegrationTests());
}

TEST(BigtableExamplesCommon, RunAdminIntegrationTestsProductionAndDisabled) {
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", {});
  google::cloud::testing_util::ScopedEnvironment admin(
      "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS", "no");
  EXPECT_FALSE(RunAdminIntegrationTests());
}

TEST(BigtableExamplesCommon, RunAdminIntegrationTestsProductionAndEnabled) {
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", {});
  google::cloud::testing_util::ScopedEnvironment admin(
      "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS", "yes");
  EXPECT_TRUE(RunAdminIntegrationTests());
}

}  // namespace examples
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
