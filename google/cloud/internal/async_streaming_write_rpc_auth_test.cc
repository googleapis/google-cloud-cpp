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

#include "google/cloud/internal/async_streaming_write_rpc_auth.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockAuthenticationStrategy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::Pair;
using ::testing::StrictMock;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string key;
  std::string value;
};

using BaseStream = AsyncStreamingWriteRpc<FakeRequest, FakeResponse>;
using AuthStream = AsyncStreamingWriteRpcAuth<FakeRequest, FakeResponse>;

class MockStream : public BaseStream {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<bool>, Write, (FakeRequest const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(future<StatusOr<FakeResponse>>, Finish, (), (override));
  MOCK_METHOD(StreamingRpcMetadata, GetRequestMetadata, (), (const, override));
};

TEST(AsyncStreamingWriteRpcAuth, Start) {
  auto factory = AuthStream::StreamFactory([](std::unique_ptr<
                                               grpc::ClientContext>) {
    auto mock = absl::make_unique<StrictMock<MockStream>>();
    EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
    EXPECT_CALL(*mock, Write).WillOnce([] { return make_ready_future(true); });
    EXPECT_CALL(*mock, WritesDone).WillOnce([] {
      return make_ready_future(true);
    });
    EXPECT_CALL(*mock, Finish).WillOnce([] {
      return make_ready_future(make_status_or(FakeResponse{"key0", "value0"}));
    });
    EXPECT_CALL(*mock, GetRequestMetadata).WillOnce([] {
      return StreamingRpcMetadata({{"test-only", "value"}});
    });
    return std::unique_ptr<BaseStream>(std::move(mock));
  });
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  EXPECT_CALL(*strategy, AsyncConfigureContext)
      .WillOnce([](std::unique_ptr<grpc::ClientContext> context) {
        return make_ready_future(make_status_or(std::move(context)));
      });
  auto uut = absl::make_unique<AuthStream>(
      absl::make_unique<grpc::ClientContext>(), strategy, factory);
  EXPECT_TRUE(uut->Start().get());
  ASSERT_TRUE(uut->Write(FakeRequest{}, grpc::WriteOptions()).get());
  ASSERT_TRUE(uut->WritesDone().get());

  auto response = uut->Finish().get();
  ASSERT_THAT(response, IsOk());
  EXPECT_EQ(response->key, "key0");
  EXPECT_EQ(response->value, "value0");
  EXPECT_THAT(uut->GetRequestMetadata(), Contains(Pair("test-only", "value")));
}

TEST(AsyncStreamingWriteRpcAuth, AuthFails) {
  auto factory =
      AuthStream::StreamFactory([](std::unique_ptr<grpc::ClientContext>) {
        auto mock = absl::make_unique<StrictMock<MockStream>>();
        EXPECT_CALL(*mock, Start).Times(0);
        EXPECT_CALL(*mock, Finish).Times(0);
        return std::unique_ptr<BaseStream>(std::move(mock));
      });
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  EXPECT_CALL(*strategy, AsyncConfigureContext)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>) {
        return make_ready_future(StatusOr<std::unique_ptr<grpc::ClientContext>>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });
  auto uut = absl::make_unique<AuthStream>(
      absl::make_unique<grpc::ClientContext>(), strategy, factory);
  EXPECT_FALSE(uut->Start().get());
  EXPECT_THAT(uut->Finish().get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(AsyncStreamingWriteRpcAuth, CancelDuringAuth) {
  auto factory = [](std::unique_ptr<grpc::ClientContext>) {
    return std::unique_ptr<BaseStream>(absl::make_unique<MockStream>());
  };
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  auto start_promise = promise<void>();
  EXPECT_CALL(*strategy, AsyncConfigureContext)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext> context) {
        struct MoveCapture {
          std::unique_ptr<grpc::ClientContext> context;
          StatusOr<std::unique_ptr<grpc::ClientContext>> operator()(
              future<void>) {
            return make_status_or(std::move(context));
          }
        };
        return start_promise.get_future().then(MoveCapture{std::move(context)});
      });

  auto uut = absl::make_unique<AuthStream>(
      absl::make_unique<grpc::ClientContext>(), strategy, factory);
  auto start = uut->Start();
  uut->Cancel();
  start_promise.set_value();
  EXPECT_FALSE(start.get());
  EXPECT_THAT(uut->Finish().get(), StatusIs(StatusCode::kInternal));
}

TEST(AsyncStreamingWriteRpcAuth, CancelAfterStart) {
  auto factory =
      AuthStream::StreamFactory([](std::unique_ptr<grpc::ClientContext>) {
        auto mock = absl::make_unique<StrictMock<MockStream>>();
        ::testing::InSequence sequence;
        EXPECT_CALL(*mock, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*mock, Write).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*mock, Cancel).Times(1);
        EXPECT_CALL(*mock, Write).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*mock, Finish).WillOnce([] {
          return make_ready_future(make_status_or(FakeResponse{"k0", "v0"}));
        });
        return std::unique_ptr<BaseStream>(std::move(mock));
      });
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  EXPECT_CALL(*strategy, AsyncConfigureContext)
      .WillOnce([](std::unique_ptr<grpc::ClientContext> context) {
        return make_ready_future(make_status_or(std::move(context)));
      });
  auto uut = absl::make_unique<AuthStream>(
      absl::make_unique<grpc::ClientContext>(), strategy, factory);
  EXPECT_TRUE(uut->Start().get());
  EXPECT_TRUE(uut->Write(FakeRequest{}, grpc::WriteOptions()).get());
  uut->Cancel();
  EXPECT_FALSE(uut->Write(FakeRequest{}, grpc::WriteOptions()).get());
  auto response = uut->Finish().get();
  ASSERT_THAT(response, IsOk());
  EXPECT_EQ(response->key, "k0");
  EXPECT_EQ(response->value, "v0");
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
