// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
namespace examples {
namespace {

using ::testing::HasSubstr;

TEST(PubSubSamplesCommon, PublisherCommand) {
  using ::google::cloud::testing_util::Usage;

  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "PUBSUB_EMULATOR_HOST", "localhost:8085");
  int call_count = 0;
  auto command = [&call_count](pubsub::Publisher const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual =
      CreatePublisherCommand("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(
      actual.second({"test-project", "test-topic", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(PubSubSamplesCommon, SubscriberCommand) {
  using ::google::cloud::testing_util::Usage;

  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "PUBSUB_EMULATOR_HOST", "localhost:8085");
  int call_count = 0;
  auto command = [&call_count](pubsub::Subscriber const&,
                               pubsub::Subscription const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual =
      CreateSubscriberCommand("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(
      actual.second({"test-project", "test-subscription", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(PubSubSamplesCommon, TopicAdminCommand) {
  using ::google::cloud::testing_util::Usage;

  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "PUBSUB_EMULATOR_HOST", "localhost:8085");
  int call_count = 0;
  auto command = [&call_count](pubsub::TopicAdminClient const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual =
      CreateTopicAdminCommand("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second({"a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(PubSubSamplesCommon, SubscriptionAdminCommand) {
  using ::google::cloud::testing_util::Usage;

  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "PUBSUB_EMULATOR_HOST", "localhost:8085");
  int call_count = 0;
  auto command = [&call_count](pubsub::SubscriptionAdminClient const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual =
      CreateSubscriptionAdminCommand("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second({"a", "b"}));
  EXPECT_EQ(1, call_count);
}

}  // namespace
}  // namespace examples
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
