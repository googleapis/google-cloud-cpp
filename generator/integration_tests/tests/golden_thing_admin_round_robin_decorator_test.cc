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

#include "generator/integration_tests/golden/internal/golden_thing_admin_round_robin_decorator.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;

auto constexpr kMockCount = 3;
auto constexpr kRepeats = 2;

std::vector<std::shared_ptr<MockGoldenThingAdminStub>> MakeMocks() {
  std::vector<std::shared_ptr<MockGoldenThingAdminStub>> mocks(kMockCount);
  std::generate(mocks.begin(), mocks.end(),
                [] { return std::make_shared<MockGoldenThingAdminStub>(); });
  return mocks;
}

std::vector<std::shared_ptr<GoldenThingAdminStub>> AsPlainStubs(
    std::vector<std::shared_ptr<MockGoldenThingAdminStub>> mocks) {
  return {mocks.begin(), mocks.end()};
}

// The general pattern of these test is to create 3 stubs and make 6 RPCs. We
// use an `InSequence` expectation to verify the request actually round-robin.
// GoldenThingAdmin has a lot of RPCs that are

TEST(GoldenThingAdminRoundRobinDecoratorTest, AsyncCreateDatabase) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncCreateDatabase)
          .WillOnce(Return(ByMove(make_ready_future(
              make_status_or(google::longrunning::Operation{})))));
    }
  }

  CompletionQueue cq;
  GoldenThingAdminRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto status =
        stub.AsyncCreateDatabase(
                cq, absl::make_unique<grpc::ClientContext>(),
                google::test::admin::database::v1::CreateDatabaseRequest{})
            .get();
    EXPECT_STATUS_OK(status);
  }
}

TEST(GoldenThingAdminRoundRobinDecoratorTest, DropDatabase) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DropDatabase).WillOnce(Return(Status{}));
    }
  }

  CompletionQueue cq;
  GoldenThingAdminRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    auto status = stub.DropDatabase(
        context, google::test::admin::database::v1::DropDatabaseRequest{});
    EXPECT_STATUS_OK(status);
  }
}

TEST(GoldenThingAdminRoundRobinDecoratorTest, AsyncGetDatabase) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncGetDatabase)
          .WillOnce(Return(ByMove(make_ready_future(
              make_status_or(google::test::admin::database::v1::Database{})))));
    }
  }

  CompletionQueue cq;
  GoldenThingAdminRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto status =
        stub.AsyncGetDatabase(
                cq, absl::make_unique<grpc::ClientContext>(),
                google::test::admin::database::v1::GetDatabaseRequest{})
            .get();
    EXPECT_STATUS_OK(status);
  }
}

TEST(GoldenThingAdminRoundRobinDecoratorTest, AsyncDropDatabase) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncDropDatabase)
          .WillOnce(Return(ByMove(make_ready_future(Status{}))));
    }
  }

  CompletionQueue cq;
  GoldenThingAdminRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto status =
        stub.AsyncDropDatabase(
                cq, absl::make_unique<grpc::ClientContext>(),
                google::test::admin::database::v1::DropDatabaseRequest{})
            .get();
    EXPECT_STATUS_OK(status);
  }
}

TEST(GoldenThingAdminRoundRobinDecoratorTest, AsyncGetOperation) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncGetOperation)
          .WillOnce(Return(ByMove(make_ready_future(
              make_status_or(google::longrunning::Operation{})))));
    }
  }

  CompletionQueue cq;
  GoldenThingAdminRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto status =
        stub.AsyncGetOperation(cq, absl::make_unique<grpc::ClientContext>(),
                               google::longrunning::GetOperationRequest{})
            .get();
    EXPECT_STATUS_OK(status);
  }
}

TEST(GoldenThingAdminRoundRobinDecoratorTest, AsyncCancelOperation) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncCancelOperation)
          .WillOnce(Return(ByMove(make_ready_future(Status{}))));
    }
  }

  CompletionQueue cq;
  GoldenThingAdminRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto status =
        stub.AsyncCancelOperation(cq, absl::make_unique<grpc::ClientContext>(),
                                  google::longrunning::CancelOperationRequest{})
            .get();
    EXPECT_STATUS_OK(status);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
