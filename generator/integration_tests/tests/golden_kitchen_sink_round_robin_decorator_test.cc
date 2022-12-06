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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_round_robin_decorator.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::test::admin::database::v1::Request;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;

auto constexpr kMockCount = 3;
auto constexpr kRepeats = 2;

std::vector<std::shared_ptr<MockGoldenKitchenSinkStub>> MakeMocks() {
  std::vector<std::shared_ptr<MockGoldenKitchenSinkStub>> mocks(kMockCount);
  std::generate(mocks.begin(), mocks.end(),
                [] { return std::make_shared<MockGoldenKitchenSinkStub>(); });
  return mocks;
}

std::vector<std::shared_ptr<GoldenKitchenSinkStub>> AsPlainStubs(
    std::vector<std::shared_ptr<MockGoldenKitchenSinkStub>> mocks) {
  return {mocks.begin(), mocks.end()};
}

// The general pattern of these test is to create 3 stubs and make 6 RPCs. We
// use an `InSequence` expectation to verify the request actually round-robin.

TEST(GoldenKitchenSinkRoundRobinDecoratorTest, GenerateAccessToken) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GenerateAccessToken)
          .WillOnce(Return(google::test::admin::database::v1::
                               GenerateAccessTokenResponse{}));
    }
  }

  GoldenKitchenSinkRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    auto status = stub.GenerateAccessToken(
        context,
        google::test::admin::database::v1::GenerateAccessTokenRequest{});
    EXPECT_STATUS_OK(status);
  }
}

TEST(GoldenKitchenSinkRoundRobinDecoratorTest, StreamingRead) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, StreamingRead).WillOnce([](auto, auto) {
        return absl::make_unique<MockStreamingReadRpc>();
      });
    }
  }

  GoldenKitchenSinkRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto stream =
        stub.StreamingRead(absl::make_unique<grpc::ClientContext>(), Request{});
    EXPECT_THAT(stream, NotNull());
  }
}

TEST(GoldenKitchenSinkRoundRobinDecoratorTest, StreamingWrite) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, StreamingWrite).WillOnce([](auto) {
        return absl::make_unique<MockStreamingWriteRpc>();
      });
    }
  }

  GoldenKitchenSinkRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto stream = stub.StreamingWrite(absl::make_unique<grpc::ClientContext>());
    EXPECT_THAT(stream, NotNull());
  }
}

TEST(GoldenKitchenSinkRoundRobinDecoratorTest, AsyncStreamingReadWrite) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncStreamingReadWrite).WillOnce([](auto&, auto) {
        return absl::make_unique<MockAsyncStreamingReadWriteRpc>();
      });
    }
  }

  CompletionQueue cq;
  GoldenKitchenSinkRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto stream = stub.AsyncStreamingReadWrite(
        cq, absl::make_unique<grpc::ClientContext>());
    EXPECT_THAT(stream, NotNull());
  }
}

TEST(GoldenKitchenSinkRoundRobinDecoratorTest, AsyncStreamingRead) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncStreamingRead).WillOnce([](auto&, auto, auto) {
        return absl::make_unique<MockAsyncStreamingReadRpc>();
      });
    }
  }

  CompletionQueue cq;
  GoldenKitchenSinkRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto stream = stub.AsyncStreamingRead(
        cq, absl::make_unique<grpc::ClientContext>(), Request{});
    EXPECT_THAT(stream, NotNull());
  }
}

TEST(GoldenKitchenSinkRoundRobinDecoratorTest, AsyncStreamingWrite) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncStreamingWrite).WillOnce([](auto&, auto) {
        return absl::make_unique<MockAsyncStreamingWriteRpc>();
      });
    }
  }

  CompletionQueue cq;
  GoldenKitchenSinkRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto stream =
        stub.AsyncStreamingWrite(cq, absl::make_unique<grpc::ClientContext>());
    EXPECT_THAT(stream, NotNull());
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
