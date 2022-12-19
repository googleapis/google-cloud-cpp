// Copyright 2022 Google LLC
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

// In order to test backwards compatibility, we only include `golden` headers.
// We do not include any headers from `golden/v1`.
#include "generator/integration_tests/golden/golden_kitchen_sink_client.h"
#include "generator/integration_tests/golden/golden_kitchen_sink_options.h"
#include "generator/integration_tests/golden/golden_thing_admin_client.h"
#include "generator/integration_tests/golden/golden_thing_admin_options.h"
#include "generator/integration_tests/golden/mocks/mock_golden_kitchen_sink_connection.h"
#include "generator/integration_tests/golden/mocks/mock_golden_thing_admin_connection.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
// In order to test backwards compatibility, we only use types from the `golden`
// namespace. We do not use any types from `golden_v1` or `golden_v1_mocks`.
namespace golden {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(ForwardingHeadersTest, BackwardsCompatForGoldenThingAdmin) {
  std::shared_ptr<GoldenThingAdminRetryPolicy> retry =
      std::make_shared<GoldenThingAdminLimitedErrorCountRetryPolicy>(5);
  retry = std::make_shared<GoldenThingAdminLimitedTimeRetryPolicy>(
      std::chrono::minutes(5));
  std::shared_ptr<GoldenThingAdminConnectionIdempotencyPolicy> idempotency =
      MakeDefaultGoldenThingAdminConnectionIdempotencyPolicy();

  auto options =
      Options{}
          .set<GoldenThingAdminPollingPolicyOption>(nullptr)
          .set<GoldenThingAdminBackoffPolicyOption>(nullptr)
          .set<GoldenThingAdminConnectionIdempotencyPolicyOption>(idempotency)
          .set<GoldenThingAdminRetryPolicyOption>(retry);

  std::shared_ptr<GoldenThingAdminConnection> conn =
      std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  GoldenThingAdminClient client(conn, options);

  std::function<std::shared_ptr<GoldenThingAdminConnection>(Options)> f =
      MakeGoldenThingAdminConnection;
  EXPECT_TRUE(static_cast<bool>(f));
}

TEST(ForwardingHeadersTest, BackwardsCompatForGoldenKitchenSink) {
  std::shared_ptr<GoldenKitchenSinkRetryPolicy> retry =
      std::make_shared<GoldenKitchenSinkLimitedErrorCountRetryPolicy>(5);
  retry = std::make_shared<GoldenKitchenSinkLimitedTimeRetryPolicy>(
      std::chrono::minutes(5));
  std::shared_ptr<GoldenKitchenSinkConnectionIdempotencyPolicy> idempotency =
      MakeDefaultGoldenKitchenSinkConnectionIdempotencyPolicy();

  auto options =
      Options{}
          .set<GoldenKitchenSinkBackoffPolicyOption>(nullptr)
          .set<GoldenKitchenSinkConnectionIdempotencyPolicyOption>(idempotency)
          .set<GoldenKitchenSinkRetryPolicyOption>(retry);

  std::shared_ptr<GoldenKitchenSinkConnection> conn =
      std::make_shared<golden_mocks::MockGoldenKitchenSinkConnection>();
  GoldenKitchenSinkClient client(conn, options);

  std::function<std::shared_ptr<GoldenKitchenSinkConnection>(Options)> f =
      MakeGoldenKitchenSinkConnection;
  EXPECT_TRUE(static_cast<bool>(f));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden
}  // namespace cloud
}  // namespace google
