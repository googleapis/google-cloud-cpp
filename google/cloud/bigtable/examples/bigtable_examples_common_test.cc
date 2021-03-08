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
#include <google/protobuf/util/time_util.h>
#include <gmock/gmock.h>
#include <stdexcept>

namespace google {
namespace cloud {
namespace bigtable {
namespace examples {

using ::testing::HasSubstr;

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

TEST(BigtableExamplesCommon, MakeTableAdminCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");
  int call_count = 0;
  auto command = [&call_count](bigtable::TableAdmin const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second({"unused", "unused", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(BigtableExamplesCommon, MakeInstanceAdminCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");

  int call_count = 0;
  auto command = [&call_count](bigtable::InstanceAdmin const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second({"unused", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(BigtableExamplesCommon, MakeTableAsyncCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");

  int call_count = 0;
  auto command = [&call_count](bigtable::Table const&, CompletionQueue const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second(
      {"unused-project", "unused-instance", "unused-table", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

}  // namespace examples
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
