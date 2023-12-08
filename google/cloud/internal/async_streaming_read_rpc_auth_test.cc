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

#include "google/cloud/internal/async_streaming_read_rpc_auth.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
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

struct FakeResponse {
  std::string key;
  std::string value;
};

using BaseStream = AsyncStreamingReadRpc<FakeResponse>;
using MockStream =
    google::cloud::testing_util::MockAsyncStreamingReadRpc<FakeResponse>;
using AuthStream = AsyncStreamingReadRpcAuth<FakeResponse>;

TEST(AsyncStreamReadWriteAuth, Start) {
  auto factory = AuthStream::StreamFactory([](auto) {
    auto mock = std::make_unique<StrictMock<MockStream>>();
    EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
    EXPECT_CALL(*mock, Read).WillOnce([] {
      return make_ready_future(absl::make_optional(FakeResponse{"k0", "v0"}));
    });
    EXPECT_CALL(*mock, Finish).WillOnce([] {
      return make_ready_future(Status{});
    });
    EXPECT_CALL(*mock, GetRequestMetadata).WillOnce([] {
      return RpcMetadata{{{"test-only", "value"}}, {{"t2", "v2"}}};
    });
    return std::unique_ptr<BaseStream>(std::move(mock));
  });
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  EXPECT_CALL(*strategy, AsyncConfigureContext).WillOnce([](auto context) {
    return make_ready_future(make_status_or(std::move(context)));
  });
  auto uut = std::make_unique<AuthStream>(
      std::make_shared<grpc::ClientContext>(), strategy, factory);
  EXPECT_TRUE(uut->Start().get());
  auto response = uut->Read().get();
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->key, "k0");
  EXPECT_EQ(response->value, "v0");
  EXPECT_THAT(uut->Finish().get(), IsOk());
  auto metadata = uut->GetRequestMetadata();
  EXPECT_THAT(metadata.headers, Contains(Pair("test-only", "value")));
  EXPECT_THAT(metadata.trailers, Contains(Pair("t2", "v2")));
}

TEST(AsyncStreamReadWriteAuth, AuthFails) {
  auto factory = AuthStream::StreamFactory([](auto) {
    auto mock = std::make_unique<StrictMock<MockStream>>();
    EXPECT_CALL(*mock, Start).Times(0);
    EXPECT_CALL(*mock, Finish).Times(0);
    return std::unique_ptr<BaseStream>(std::move(mock));
  });
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  EXPECT_CALL(*strategy, AsyncConfigureContext).WillOnce([](auto) {
    return make_ready_future(StatusOr<std::shared_ptr<grpc::ClientContext>>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  });
  auto uut = std::make_unique<AuthStream>(
      std::make_shared<grpc::ClientContext>(), strategy, factory);
  EXPECT_FALSE(uut->Start().get());
  EXPECT_THAT(uut->Finish().get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(AsyncStreamReadWriteAuth, CancelDuringAuth) {
  auto factory = [](auto) {
    return std::unique_ptr<BaseStream>(std::make_unique<MockStream>());
  };
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  auto start_promise = promise<void>();
  EXPECT_CALL(*strategy, AsyncConfigureContext).WillOnce([&](auto context) {
    return start_promise.get_future().then(
        [c = std::move(context)](auto) mutable {
          return make_status_or(std::move(c));
        });
  });

  auto uut = std::make_unique<AuthStream>(
      std::make_shared<grpc::ClientContext>(), strategy, factory);
  auto start = uut->Start();
  uut->Cancel();
  start_promise.set_value();
  EXPECT_FALSE(start.get());
  EXPECT_THAT(uut->Finish().get(), StatusIs(StatusCode::kInternal));
}

TEST(AsyncStreamReadWriteAuth, CancelAfterStart) {
  auto factory = AuthStream::StreamFactory([](auto) {
    auto mock = std::make_unique<StrictMock<MockStream>>();
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
    EXPECT_CALL(*mock, Read).WillOnce([] {
      return make_ready_future(absl::make_optional(FakeResponse{"k0", "v0"}));
    });
    EXPECT_CALL(*mock, Cancel).Times(1);
    EXPECT_CALL(*mock, Read).WillOnce([] {
      return make_ready_future(absl::optional<FakeResponse>{});
    });
    EXPECT_CALL(*mock, Finish).WillOnce([] {
      return make_ready_future(Status{});
    });
    return std::unique_ptr<BaseStream>(std::move(mock));
  });
  auto strategy = std::make_shared<StrictMock<MockAuthenticationStrategy>>();
  EXPECT_CALL(*strategy, AsyncConfigureContext).WillOnce([](auto context) {
    return make_ready_future(make_status_or(std::move(context)));
  });
  auto uut = std::make_unique<AuthStream>(
      std::make_shared<grpc::ClientContext>(), strategy, factory);
  EXPECT_TRUE(uut->Start().get());
  auto response = uut->Read().get();
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->key, "k0");
  EXPECT_EQ(response->value, "v0");
  uut->Cancel();
  response = uut->Read().get();
  EXPECT_FALSE(response);
  EXPECT_THAT(uut->Finish().get(), IsOk());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
