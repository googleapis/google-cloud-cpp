// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/subscriber_options.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/scoped_log.h"
// This file contains the tests for deprecated functions, we need to disable
// the warnings
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using std::chrono::seconds;
using ::testing::Contains;
using ::testing::HasSubstr;

TEST(SubscriberOptionsTest, Default) {
  SubscriberOptions const options{};
  EXPECT_EQ(seconds(600), options.max_deadline_extension());
  EXPECT_LT(0, options.max_outstanding_messages());
  EXPECT_LT(0, options.max_outstanding_bytes());
  EXPECT_LT(0, options.max_concurrency());
}

TEST(SubscriberOptionsTest, SetDeadlineExtension) {
  auto options = SubscriberOptions{}.set_max_deadline_extension(seconds(60));
  EXPECT_EQ(seconds(60), options.max_deadline_extension());

  options.set_max_deadline_extension(seconds(5));
  EXPECT_EQ(seconds(10), options.max_deadline_extension());

  options.set_max_deadline_extension(seconds(1000));
  EXPECT_EQ(seconds(600), options.max_deadline_extension());
}

TEST(SubscriberOptionsTest, SetMessageCount) {
  auto options = SubscriberOptions{}.set_max_outstanding_messages(16);
  EXPECT_EQ(16, options.max_outstanding_messages());

  options.set_max_outstanding_messages(-7);
  EXPECT_EQ(0, options.max_outstanding_messages());
}

TEST(SubscriberOptionsTest, SetBytes) {
  auto options = SubscriberOptions{}.set_max_outstanding_bytes(16 * 1024);
  EXPECT_EQ(16 * 1024, options.max_outstanding_bytes());

  options.set_max_outstanding_bytes(-7);
  EXPECT_EQ(0, options.max_outstanding_bytes());
}

TEST(SubscriberOptionsTest, SetConcurrency) {
  auto options = SubscriberOptions{}.set_max_concurrency(16);
  EXPECT_EQ(16, options.max_concurrency());

  // 0 resets to default
  options.set_max_concurrency(0);
  EXPECT_EQ(SubscriberOptions{}.max_concurrency(), options.max_concurrency());
}

TEST(SubscriberOptionsTest, OptionsConstructor) {
  auto const options =
      SubscriberOptions(Options{}
                            .set<MaxDeadlineTimeOption>(seconds(2))
                            .set<MaxDeadlineExtensionOption>(seconds(30))
                            .set<MaxOutstandingMessagesOption>(4)
                            .set<MaxOutstandingBytesOption>(5)
                            .set<MaxConcurrencyOption>(6));

  EXPECT_EQ(seconds(2), options.max_deadline_time());
  EXPECT_EQ(seconds(30), options.max_deadline_extension());
  EXPECT_EQ(4, options.max_outstanding_messages());
  EXPECT_EQ(5, options.max_outstanding_bytes());
  EXPECT_EQ(6, options.max_concurrency());
}

TEST(SubscriberOptionsTest, ExpectedOptionsCheck) {
  struct NonOption {
    using Type = bool;
  };

  testing_util::ScopedLog log;
  auto options = SubscriberOptions(Options{}.set<NonOption>(true));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("Unexpected option")));
}

TEST(SubscriberOptionsTest, MakeOptions) {
  auto options = SubscriberOptions{}
                     .set_max_deadline_time(seconds(2))
                     .set_max_deadline_extension(seconds(30))
                     .set_max_outstanding_messages(4)
                     .set_max_outstanding_bytes(5)
                     .set_max_concurrency(6);

  auto opts = pubsub_internal::MakeOptions(std::move(options));
  EXPECT_EQ(seconds(2), opts.get<MaxDeadlineTimeOption>());
  EXPECT_EQ(seconds(30), opts.get<MaxDeadlineExtensionOption>());
  EXPECT_EQ(4, opts.get<MaxOutstandingMessagesOption>());
  EXPECT_EQ(5, opts.get<MaxOutstandingBytesOption>());
  EXPECT_EQ(6, opts.get<MaxConcurrencyOption>());

  // Ensure that we are not setting any extra options
  EXPECT_FALSE(opts.has<GrpcCredentialOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
