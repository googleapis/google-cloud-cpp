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

#include "generator/integration_tests/golden/v1/golden_thing_admin_client.h"
#include "generator/integration_tests/golden/v1/golden_thing_admin_options.h"
#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_connection_impl.h"
#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_option_defaults.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MockBackoffPolicy;
using ::testing::AtLeast;
using ::testing::Return;
using ms = std::chrono::milliseconds;

class MockRetryPolicy : public GoldenThingAdminRetryPolicy {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
  MOCK_METHOD(std::unique_ptr<GoldenThingAdminRetryPolicy>, clone, (),
              (const, override));
};

TEST(PlumbingTest, RetryLoopUsesPerCallPolicies) {
  auto call_r = std::make_shared<MockRetryPolicy>();
  auto call_b = std::make_shared<MockBackoffPolicy>();
  auto call_options = Options{}
                          .set<GoldenThingAdminRetryPolicyOption>(call_r)
                          .set<GoldenThingAdminBackoffPolicyOption>(call_b);

  EXPECT_CALL(*call_r, clone).WillOnce([] {
    auto clone = std::make_unique<MockRetryPolicy>();
    // We will just say the policy is never exhausted, and use a permanent error
    // to break out of the loop.
    EXPECT_CALL(*clone, IsExhausted)
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*clone, OnFailure)
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    return clone;
  });

  EXPECT_CALL(*call_b, clone).WillOnce([] {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).WillOnce(Return(ms(0)));
    return clone;
  });

  auto stub = std::make_shared<golden_v1_internal::MockGoldenThingAdminStub>();
  EXPECT_CALL(*stub, GetDatabase)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "fail")));

  auto options = golden_v1_internal::GoldenThingAdminDefaultOptions({});
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  auto conn =
      std::make_shared<golden_v1_internal::GoldenThingAdminConnectionImpl>(
          std::move(background), std::move(stub), std::move(options));
  auto client = GoldenThingAdminClient(std::move(conn));

  (void)client.GetDatabase("name", std::move(call_options));
}

class MockPollingPolicy : public PollingPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<PollingPolicy>, clone, (), (const, override));
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(std::chrono::milliseconds, WaitPeriod, (), (override));
};

TEST(PlumbingTest, PollingLoopUsesPerCallPolicies) {
  auto call_p = std::make_shared<MockPollingPolicy>();
  auto call_options =
      Options{}.set<GoldenThingAdminPollingPolicyOption>(call_p);

  EXPECT_CALL(*call_p, clone).WillOnce([] {
    auto clone = std::make_unique<MockPollingPolicy>();
    EXPECT_CALL(*clone, WaitPeriod).WillOnce(Return(ms(0)));
    return clone;
  });

  auto stub = std::make_shared<golden_v1_internal::MockGoldenThingAdminStub>();
  EXPECT_CALL(*stub, AsyncCreateDatabase).WillOnce([] {
    google::longrunning::Operation op;
    op.set_name("test-operation-name");
    op.set_done(false);
    return make_ready_future(make_status_or(op));
  });
  EXPECT_CALL(*stub, AsyncGetOperation).WillOnce([] {
    google::longrunning::Operation op;
    op.set_name("test-operation-name");
    op.set_done(true);
    return make_ready_future(make_status_or(op));
  });

  auto options = golden_v1_internal::GoldenThingAdminDefaultOptions({});
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  auto conn =
      std::make_shared<golden_v1_internal::GoldenThingAdminConnectionImpl>(
          std::move(background), std::move(stub), std::move(options));
  auto client = GoldenThingAdminClient(std::move(conn));

  (void)client.CreateDatabase({}, std::move(call_options));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
