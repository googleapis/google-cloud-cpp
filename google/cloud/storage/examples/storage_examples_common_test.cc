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

#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace examples {

using ::testing::HasSubstr;
using ::testing::StartsWith;

TEST(StorageExamplesCommon, Simple) {
  int test_calls = 0;
  Example example({
      {"test",
       [&](std::vector<std::string> const& args) {
         ++test_calls;
         if (args.empty()) throw Usage("test-usage");
         if (args[0] == "--help") throw Usage("usage with --help");
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

TEST(StorageExamplesCommon, AutoRunDisabled) {
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

TEST(StorageExamplesCommon, AutoRunMissing) {
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

TEST(StorageExamplesCommon, AutoRun) {
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

TEST(StorageExamplesCommon, CommandNotFound) {
  int test_calls = 0;
  Example example({
      {"test", [&](std::vector<std::string> const&) { ++test_calls; }},
  });
  char const* argv[] = {"argv0", "wrong-name"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 1);
  EXPECT_EQ(1, test_calls);
}

TEST(StorageExamplesCommon, CommandUsage) {
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

TEST(StorageExamplesCommon, CommandError) {
  int test_calls = 0;
  Example example({
      {"test",
       [&](std::vector<std::string> const& args) {
         ++test_calls;
         if (args.empty()) throw Usage("test-usage");
         if (args[0] == "--help") throw Usage("usage with --help");
         throw std::runtime_error("some problem");
       }},
  });
  char const* argv[] = {"argv0", "test", "a0"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_EQ(example.Run(argc, argv), 1);
  EXPECT_EQ(2, test_calls);
}

TEST(StorageExamplesCommon, UsingTestbenchTrue) {
  google::cloud::testing_util::ScopedEnvironment env(
      "CLOUD_STORAGE_TESTBENCH_ENDPOINT", "http://localhost:9090");
  EXPECT_TRUE(UsingTestbench());
}

TEST(StorageExamplesCommon, UsingTestbenchFalse) {
  google::cloud::testing_util::ScopedEnvironment env(
      "CLOUD_STORAGE_TESTBENCH_ENDPOINT", {});
  EXPECT_FALSE(UsingTestbench());
}

TEST(StorageExamplesCommon, CheckEnvironmentVariablesNormal) {
  google::cloud::testing_util::ScopedEnvironment test_a("TEST_A", "a");
  google::cloud::testing_util::ScopedEnvironment test_b("TEST_B", "b");
  EXPECT_NO_THROW(CheckEnvironmentVariablesAreSet({"TEST_A", "TEST_B"}));
}

TEST(StorageExamplesCommon, CheckEnvironmentVariablesNotSet) {
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

TEST(StorageExamplesCommon, CheckEnvironmentVariablesSetEmpty) {
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

TEST(StorageExamplesCommon, RandomBucket) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const actual_1 = MakeRandomBucketName(generator, "test-prefix-");
  EXPECT_THAT(actual_1, StartsWith("test-prefix-"));
  auto const actual_2 = MakeRandomBucketName(generator, "test-prefix-");
  EXPECT_THAT(actual_2, StartsWith("test-prefix-"));
  EXPECT_NE(actual_1, actual_2);
}

TEST(StorageExamplesCommon, RandomObject) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const actual_1 = MakeRandomObjectName(generator, "test-prefix-");
  EXPECT_THAT(actual_1, StartsWith("test-prefix-"));
  auto const actual_2 = MakeRandomObjectName(generator, "test-prefix-");
  EXPECT_THAT(actual_2, StartsWith("test-prefix-"));
  EXPECT_NE(actual_1, actual_2);
}

TEST(StorageExamplesCommon, CreateCommandEntryUsage) {
  // Set the client to use the testbench, this avoids any problems trying to
  // find and load the default credentials file.
  google::cloud::testing_util::ScopedEnvironment env(
      "CLOUD_STORAGE_TESTBENCH_ENDPOINT", "http://localhost:9090");

  int call_count = 0;
  auto command = [&call_count](google::cloud::storage::Client const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("1", argv.at(0));
    EXPECT_EQ("2", argv.at(1));
  };
  auto entry = CreateCommandEntry("my-test", {"foo", "bar"}, command);
  EXPECT_EQ("my-test", entry.first);

  EXPECT_THROW(
      try { entry.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("my-test foo bar"));
        throw;
      },
      Usage);

  EXPECT_EQ(0, call_count);
  entry.second({"1", "2"});
  EXPECT_EQ(1, call_count);
}

TEST(StorageExamplesCommon, CreateCommandEntryNoArguments) {
  // Set the client to use the testbench, this avoids any problems trying to
  // find and load the default credentials file.
  google::cloud::testing_util::ScopedEnvironment env(
      "CLOUD_STORAGE_TESTBENCH_ENDPOINT", "http://localhost:9090");

  int call_count = 0;
  auto command = [&call_count](google::cloud::storage::Client const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("1", argv.at(0));
    EXPECT_EQ("2", argv.at(1));
  };
  auto entry = CreateCommandEntry("my-test", {"foo", "bar"}, command);
  EXPECT_EQ("my-test", entry.first);

  EXPECT_THROW(
      try { entry.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("my-test foo bar"));
        throw;
      },
      Usage);

  EXPECT_EQ(0, call_count);
  entry.second({"1", "2"});
  EXPECT_EQ(1, call_count);

  // Too many args when not using varargs is an error.
  EXPECT_THROW(
      try {
        entry.second({"1", "2", "3"});
      } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("my-test foo bar"));
        throw;
      },
      Usage);
  EXPECT_EQ(1, call_count);
}

TEST(StorageExamplesCommon, CreateCommandEntryVarargs) {
  // Set the client to use the testbench, this avoids any problems trying to
  // find and load the default credentials file.
  google::cloud::testing_util::ScopedEnvironment env(
      "CLOUD_STORAGE_TESTBENCH_ENDPOINT", "http://localhost:9090");

  int call_count = 0;
  auto command = [&call_count](google::cloud::storage::Client const&,
                               std::vector<std::string> const& argv) {
    switch (++call_count) {
      case 1:
        ASSERT_EQ(2, argv.size());
        EXPECT_EQ("1", argv.at(0));
        EXPECT_EQ("fixed", argv.at(1));
        break;
      case 2:
        ASSERT_EQ(4, argv.size());
        EXPECT_EQ("1", argv.at(0));
        EXPECT_EQ("var", argv.at(1));
        EXPECT_EQ("3", argv.at(2));
        EXPECT_EQ("4", argv.at(3));
        break;
      default:
        FAIL() << "command called more times than expected";
        break;
    }
  };
  auto entry =
      CreateCommandEntry("my-test", {"foo", "bar", "[bar...]"}, command);
  EXPECT_EQ("my-test", entry.first);

  EXPECT_THROW(
      try { entry.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("my-test foo bar [bar...]"));
        throw;
      },
      Usage);

  EXPECT_EQ(0, call_count);
  ASSERT_NO_FATAL_FAILURE(entry.second({"1", "fixed"}));
  EXPECT_EQ(1, call_count);
  ASSERT_NO_FATAL_FAILURE(entry.second({"1", "var", "3", "4"}));
  EXPECT_EQ(2, call_count);
}

}  // namespace examples
}  // namespace storage
}  // namespace cloud
}  // namespace google
