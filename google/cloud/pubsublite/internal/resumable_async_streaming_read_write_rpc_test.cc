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

#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <deque>
#include <iostream>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::_;
using ::testing::StrictMock;

using ::google::cloud::internal::RetryPolicy;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string key;
  std::string value;
};

// Used as part of the EXPECT_CALL() below:
bool operator==(FakeRequest const& lhs, FakeRequest const& rhs) {
  return lhs.key == rhs.key;
}

bool operator==(FakeResponse const& lhs, FakeResponse const& rhs) {
  return lhs.key == rhs.key && lhs.value == rhs.value;
}

using MockAsyncStreamReturnType =
    std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

class MockAsyncReaderWriter
    : public AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse> {
 public:
  MOCK_METHOD(future<absl::optional<FakeResponse>>, Read, (), (override));
  MOCK_METHOD(future<bool>, Write, (FakeRequest const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
};

class MockRetryPolicy : public RetryPolicy {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
};

class MockStub {
 public:
  MOCK_METHOD(MockAsyncStreamReturnType, FakeStream, (), ());
  MOCK_METHOD(std::unique_ptr<RetryPolicy>, FakeRetryPolicy, (), ());
};

std::unique_ptr<BackoffPolicy> DefaultBackoffPolicy() {
  using us = std::chrono::microseconds;
  return ExponentialBackoffPolicy(/*initial_delay=*/us(10),
                                  /*maximum_delay=*/us(40), /*scaling=*/2.0)
      .clone();
}

future<void> MockSleeper(std::chrono::duration<double>) {
  return make_ready_future();
}

template <typename T>
void SetOpValue(std::deque<std::shared_ptr<promise<T>>>& operations,
                T response) {
  auto op = std::move(operations.front());
  operations.pop_front();
  op->set_value(response);
}

TEST(AsyncReadWriteStreamingRpcTest, BasicReadWriteGood) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream).WillOnce([]() {
    auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _)).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Read)
        .WillOnce([]() {
          return make_ready_future(
              absl::make_optional(FakeResponse{"key0", "value0_0"}));
        })
        .WillOnce([]() {
          return make_ready_future(
              absl::make_optional(FakeResponse{"key0", "value0_1"}));
        });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(make_ready_future(Status()));
    });
    return stream;
  });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  EXPECT_CALL(mock, FakeRetryPolicy).WillOnce([]() {
    return absl::make_unique<StrictMock<MockRetryPolicy>>();
  });

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  auto write = stream->Write(FakeRequest{"key0"},
                             grpc::WriteOptions().set_last_message());
  ASSERT_TRUE(write.get());

  auto response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, (FakeResponse{"key0", "value0_0"}));

  response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, (FakeResponse{"key0", "value0_1"}));

  auto finish = stream->Finish();
  finish.get();

  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, SingleReadFailureThenGood) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _)).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(absl::optional<FakeResponse>());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(
              Status(StatusCode::kUnavailable, "Unavailable")));
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([]() {
              return make_ready_future(
                  absl::make_optional(FakeResponse{"key0", "value0_0"}));
            })
            .WillOnce([]() {
              return make_ready_future(
                  absl::make_optional(FakeResponse{"key0", "value0_1"}));
            });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(Status()));
        });
        return stream;
      });

  EXPECT_CALL(mock, FakeRetryPolicy)
      // Initialize call
      .WillOnce(
          []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); })
      .WillOnce([]() {
        auto mock_retry_policy =
            absl::make_unique<StrictMock<MockRetryPolicy>>();
        EXPECT_CALL(*mock_retry_policy, IsExhausted).WillOnce([]() {
          return false;
        });
        EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillOnce([]() {
          return true;
        });
        return mock_retry_policy;
      });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  ASSERT_TRUE(
      stream
          ->Write(FakeRequest{"key0"}, grpc::WriteOptions().set_last_message())
          .get());

  auto response0 = stream->Read().get();
  ASSERT_FALSE(response0.has_value());

  response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, (FakeResponse{"key0", "value0_0"}));

  response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, (FakeResponse{"key0", "value0_1"}));

  auto finish = stream->Finish();
  finish.get();

  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, SingleStartFailureThenGood) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(
              Status(StatusCode::kUnavailable, "Unavailable")));
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(
              absl::make_optional(FakeResponse{"key0", "value0_0"}));
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(Status()));
        });
        return stream;
      });

  EXPECT_CALL(mock, FakeRetryPolicy).WillOnce([]() {
    auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
    EXPECT_CALL(*mock_retry_policy, IsExhausted).WillOnce([]() {
      return false;
    });
    EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillOnce([]() {
      return true;
    });
    return mock_retry_policy;
  });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  auto response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, (FakeResponse{"key0", "value0_0"}));

  auto finish = stream->Finish();
  finish.get();

  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishInMiddleOfWrite) {
  StrictMock<MockStub> mock;
  promise<bool> write_promise;
  EXPECT_CALL(mock, FakeStream).WillOnce([&write_promise]() {
    auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _))
        .WillOnce([&write_promise]() { return write_promise.get_future(); });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(make_ready_future(Status()));
    });
    return stream;
  });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  EXPECT_CALL(mock, FakeRetryPolicy).WillOnce([]() {
    return absl::make_unique<StrictMock<MockRetryPolicy>>();
  });

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();
  auto write = stream->Write(FakeRequest{"key0"},
                             grpc::WriteOptions().set_last_message());
  auto finish_future = stream->Finish();
  write_promise.set_value(true);
  ASSERT_FALSE(write.get());

  finish_future.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishInMiddleOfRetryAfterStart) {
  StrictMock<MockStub> mock;
  promise<bool> start_promise;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _)).WillOnce([]() {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(
              Status(StatusCode::kUnavailable, "Unavailable")));
        });
        return stream;
      })
      .WillOnce([&start_promise]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([&start_promise]() {
          return start_promise.get_future();
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(Status()));
        });
        return stream;
      });

  EXPECT_CALL(mock, FakeRetryPolicy)
      // Initialize call
      .WillOnce(
          []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); })
      .WillOnce([]() {
        auto mock_retry_policy =
            absl::make_unique<StrictMock<MockRetryPolicy>>();
        EXPECT_CALL(*mock_retry_policy, IsExhausted).WillRepeatedly([]() {
          return false;
        });
        EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillRepeatedly([]() {
          return true;
        });
        return mock_retry_policy;
      });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  auto write = stream->Write(FakeRequest{"key0"},
                             grpc::WriteOptions().set_last_message());

  auto finish = stream->Finish();
  start_promise.set_value(true);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishInMiddleOfRetryBeforeStart) {
  StrictMock<MockStub> mock;
  promise<Status> finish_promise;
  EXPECT_CALL(mock, FakeStream).WillOnce([&finish_promise]() {
    auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _)).WillOnce([]() {
      return make_ready_future(false);
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&finish_promise]() {
      return finish_promise.get_future();
    });
    return stream;
  });

  EXPECT_CALL(mock, FakeRetryPolicy)
      // Initialize call
      .WillOnce(
          []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); })
      .WillOnce([]() {
        auto mock_retry_policy =
            absl::make_unique<StrictMock<MockRetryPolicy>>();
        return mock_retry_policy;
      });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  auto write = stream->Write(FakeRequest{"key0"},
                             grpc::WriteOptions().set_last_message());

  auto finish = stream->Finish();
  finish_promise.set_value(Status(StatusCode::kUnavailable, "Unavailable"));
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishWhileShutdown) {
  StrictMock<MockStub> mock;
  const Status fail_status = Status(StatusCode::kUnavailable, "Unavailable");
  EXPECT_CALL(mock, FakeStream).WillOnce([&fail_status]() {
    auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _)).WillOnce([]() {
      return make_ready_future(false);
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&fail_status]() {
      return make_ready_future(make_ready_future(fail_status));
    });
    return stream;
  });

  EXPECT_CALL(mock, FakeRetryPolicy)
      // Initialize call
      .WillOnce(
          []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); })
      .WillOnce([]() {
        auto mock_retry_policy =
            absl::make_unique<StrictMock<MockRetryPolicy>>();
        EXPECT_CALL(*mock_retry_policy, IsExhausted).WillOnce([]() {
          return true;
        });
        return mock_retry_policy;
      });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  ASSERT_FALSE(
      stream
          ->Write(FakeRequest{"key0"}, grpc::WriteOptions().set_last_message())
          .get());

  auto finish = stream->Finish();
  finish.get();

  EXPECT_EQ(start.get(), Status(StatusCode::kUnavailable, "Unavailable"));
}

TEST(AsyncReadWriteStreamingRpcTest, ReadFailWhileWriteInFlight) {
  StrictMock<MockStub> mock;
  promise<bool> write_promise;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([&write_promise]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _))
            .WillOnce(
                [&write_promise]() { return write_promise.get_future(); });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(absl::optional<FakeResponse>());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(
              Status(StatusCode::kUnavailable, "Unavailable")));
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<MockAsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(
              absl::make_optional(FakeResponse{"key0", "value0_0"}));
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(make_ready_future(Status()));
        });
        return stream;
      });

  EXPECT_CALL(mock, FakeRetryPolicy)
      // Initialize call
      .WillOnce(
          []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); })
      .WillOnce([]() {
        auto mock_retry_policy =
            absl::make_unique<StrictMock<MockRetryPolicy>>();
        EXPECT_CALL(*mock_retry_policy, IsExhausted).WillOnce([]() {
          return false;
        });
        EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillOnce([]() {
          return true;
        });
        return mock_retry_policy;
      });

  std::shared_ptr<BackoffPolicy const> backoff_policy = DefaultBackoffPolicy();

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  auto write = stream->Write(FakeRequest{"key0"},
                             grpc::WriteOptions().set_last_message());

  auto response0_fut = stream->Read();
  write_promise.set_value(true);
  ASSERT_FALSE(response0_fut.get().has_value());
  ASSERT_TRUE(write.get());

  auto response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, (FakeResponse{"key0", "value0_0"}));

  auto finish = stream->Finish();
  finish.get();

  EXPECT_THAT(start.get(), IsOk());
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
