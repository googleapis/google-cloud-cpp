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
#include "google/cloud/pubsublite/internal/mock_async_reader_writer.h"
#include "google/cloud/pubsublite/internal/mock_retry_policy.h"
#include "google/cloud/pubsublite/internal/mock_backoff_policy.h"
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
using ::testing::MockFunction;

using ::google::cloud::pubsublite_internal::MockRetryPolicy;
using ::google::cloud::pubsublite_internal::MockBackoffPolicy;

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

using AsyncReaderWriter = ::google::cloud::pubsublite_internal::MockAsyncReaderWriter<
    FakeRequest,
    FakeResponse>;
using AsyncReadWriteStream = std::unique_ptr<AsyncReaderWriter>;

using AsyncReadWriteStreamReturnType =
std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

class MockStub {
 public:
  MOCK_METHOD(AsyncReadWriteStream, FakeStream, (), ());
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

const Status kFailStatus = Status(StatusCode::kUnavailable, "Unavailable");
const FakeResponse kBasicResponse = FakeResponse{"key0", "value0_0"};
const FakeResponse kBasicResponse1 = FakeResponse{"key0", "value0_1"};
const FakeRequest kBasicRequest = FakeRequest{"key0"};

class ResumableAsyncReadWriteStreamingRpcTest : public ::testing::Test {
 protected:
  ResumableAsyncReadWriteStreamingRpcTest() : backoff_policy_{
      std::make_shared<StrictMock<MockBackoffPolicy>>()}, stream_{
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [this]() { return retry_policy_factory_.Call(); },
          backoff_policy_,
          [this](std::chrono::duration<double> t) { return sleeper_.Call(t); },
          [this]() { return stream_factory_.Call(); },
          [this](AsyncReadWriteStreamReturnType stream) {
            return initializer_.Call(std::move(stream));
          })} {
  }

  StrictMock<MockFunction<future<void>(std::chrono::duration<double>)>>
      sleeper_;
  StrictMock<MockFunction<AsyncReadWriteStream()>> stream_factory_;
  StrictMock<MockFunction<std::unique_ptr<MockRetryPolicy>()>>
      retry_policy_factory_;
  StrictMock<MockFunction<future<StatusOr<AsyncReadWriteStreamReturnType>>(
      AsyncReadWriteStreamReturnType)>>
      initializer_;
  std::shared_ptr<StrictMock<MockBackoffPolicy>> backoff_policy_;
  ResumableAsyncReadWriteStream stream_;

  void InitializerBehavior(unsigned int times) {
    EXPECT_CALL(initializer_, Call).Times(times).WillRepeatedly([](
        AsyncReadWriteStreamReturnType stream) {
      return make_ready_future(
          StatusOr<AsyncReadWriteStreamReturnType>(std::move(stream)));
    });
  }

  void NoRetryInvocation() {
    EXPECT_CALL(retry_policy_factory_,
                Call).WillOnce(Return(ByMove(absl::make_unique<StrictMock<
        MockRetryPolicy>>())));
  }

  void BackoffPolicyBehavior(unsigned int clone_times,
                             unsigned int on_completion_times) {
    EXPECT_CALL(*backoff_policy_,
                clone).Times(clone_times).WillRepeatedly([on_completion_times]() {
      auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
      if (on_completion_times > 0) {
        EXPECT_CALL(*backoff_policy,
                    OnCompletion).Times(on_completion_times).WillRepeatedly([]() {
          return std::chrono::milliseconds(0);
        });
      }
      return backoff_policy;
    });
  }
};

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, BasicReadWriteGood) {
  EXPECT_CALL(stream_factory_, Call).WillOnce([]() {
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Read)
        .WillOnce([]() {
          return make_ready_future(
              absl::make_optional(kBasicResponse));
        })
        .WillOnce([]() {
          return make_ready_future(
              absl::make_optional(kBasicResponse1));
        });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(Status());
    });
    return stream;
  });

  InitializerBehavior(1);
  NoRetryInvocation();
  BackoffPolicyBehavior(1, 0);

  auto start = stream_->Start();
  ASSERT_TRUE(
      stream_
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse1);
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, ReadWriteAfterShutdown) {
  EXPECT_CALL(stream_factory_, Call).WillOnce([]() {
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(Status());
    });
    return stream;
  });

  InitializerBehavior(1);
  NoRetryInvocation();
  BackoffPolicyBehavior(1, 0);

  auto start = stream_->Start();
  auto finish = stream_->Finish();
  finish.get();
  ASSERT_FALSE(
      stream_
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  ASSERT_FALSE(stream_->Read().get().has_value());
  EXPECT_THAT(start.get(), IsOk());
}

void ExpectStandardSingleRetryPolicy(MockStub& mock) {
  EXPECT_CALL(mock, FakeRetryPolicy)
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
}

#define INIT_BASIC_STREAM \
auto stream = \
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>( \
          [&mock]() { return mock.FakeRetryPolicy(); }, DefaultBackoffPolicy(), \
          &MockSleeper, [&mock]() { return mock.FakeStream(); }, \
          [](AsyncReadWriteStreamReturnType stream) { \
            return make_ready_future( \
                StatusOr<AsyncReadWriteStreamReturnType>(std::move(stream))); \
          });

TEST(AsyncReadWriteStreamingRpcTest, ReadWriteAfterShutdown) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream).WillOnce([]() {
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(Status());
    });
    return stream;
  });

  EXPECT_CALL(mock, FakeRetryPolicy).WillOnce([]() {
    return absl::make_unique<StrictMock<MockRetryPolicy>>();
  });

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto finish = stream->Finish();
  finish.get();
  ASSERT_FALSE(
      stream
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  ASSERT_FALSE(stream->Read().get().has_value());
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, SingleReadFailureThenGood) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(absl::optional<FakeResponse>());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([]() {
              return make_ready_future(
                  absl::make_optional(kBasicResponse));
            })
            .WillOnce([]() {
              return make_ready_future(
                  absl::make_optional(kBasicResponse1));
            });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(Status());
        });
        return stream;
      });

  ExpectStandardSingleRetryPolicy(mock);

  INIT_BASIC_STREAM

  auto start = stream->Start();
  ASSERT_TRUE(
      stream
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  auto response0 = stream->Read().get();
  ASSERT_FALSE(response0.has_value());
  response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse1);
  auto finish = stream->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, SingleStartFailureThenGood) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(
              absl::make_optional(kBasicResponse));
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(Status());
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

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  auto finish = stream->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishInMiddleOfReadWrite) {
  StrictMock<MockStub> mock;
  promise<bool> write_promise;
  promise<absl::optional<FakeResponse>> read_promise;
  EXPECT_CALL(mock, FakeStream).WillOnce([&write_promise, &read_promise]() {
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(kBasicRequest, _))
        .WillOnce([&write_promise]() { return write_promise.get_future(); });
    EXPECT_CALL(*stream, Read).WillOnce([&read_promise]() {
      return read_promise.get_future();
    });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(Status());
    });
    return stream;
  });

  EXPECT_CALL(mock, FakeRetryPolicy).WillOnce([]() {
    return absl::make_unique<StrictMock<MockRetryPolicy>>();
  });

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto write = stream->Write(kBasicRequest,
                             grpc::WriteOptions().set_last_message());
  auto read = stream->Read();
  auto finish_future = stream->Finish();
  write_promise.set_value(true);
  read_promise.set_value(absl::make_optional(kBasicResponse));
  ASSERT_FALSE(write.get());
  ASSERT_FALSE(read.get().has_value());
  finish_future.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishInMiddleOfRetryAfterStart) {
  StrictMock<MockStub> mock;
  promise<bool> start_promise;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      })
      .WillOnce([&start_promise]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([&start_promise]() {
          return start_promise.get_future();
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(Status());
        });
        return stream;
      });

  ExpectStandardSingleRetryPolicy(mock);

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto write = stream->Write(kBasicRequest,
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
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
      return make_ready_future(false);
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&finish_promise]() {
      return finish_promise.get_future();
    });
    return stream;
  });

  EXPECT_CALL(mock, FakeRetryPolicy)
      .WillOnce(
          []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); })
      .WillOnce([]() {
        auto mock_retry_policy =
            absl::make_unique<StrictMock<MockRetryPolicy>>();
        return mock_retry_policy;
      });

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto write = stream->Write(kBasicRequest,
                             grpc::WriteOptions().set_last_message());
  auto finish = stream->Finish();
  finish_promise.set_value(kFailStatus);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishInMiddleOfRetryDuringSleep) {
  StrictMock<MockStub> mock;
  promise<void> sleep_promise;
  EXPECT_CALL(mock, FakeStream).WillOnce([]() {
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
      return make_ready_future(false);
    });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(kFailStatus);
    });
    return stream;
  });

  ExpectStandardSingleRetryPolicy(mock);

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          [&mock]() { return mock.FakeRetryPolicy(); }, DefaultBackoffPolicy(),
          [&sleep_promise](std::chrono::duration<double>) {
            return sleep_promise.get_future();
          },
          [&mock]() { return mock.FakeStream(); },
          [](AsyncReadWriteStreamReturnType stream) {
            return make_ready_future(
                StatusOr<AsyncReadWriteStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();
  auto write = stream->Write(kBasicRequest,
                             grpc::WriteOptions().set_last_message());
  auto finish = stream->Finish();
  sleep_promise.set_value();
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, FinishWhileShutdown) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream).WillOnce([]() {
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
      return make_ready_future(false);
    });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(kFailStatus);
    });
    return stream;
  });

  EXPECT_CALL(mock, FakeRetryPolicy)
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

  INIT_BASIC_STREAM

  auto start = stream->Start();
  ASSERT_FALSE(
      stream
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  EXPECT_EQ(start.get(), kFailStatus);
  auto finish = stream->Finish();
  finish.get();
}

TEST(AsyncReadWriteStreamingRpcTest, ReadFailWhileWriteInFlight) {
  StrictMock<MockStub> mock;
  promise<bool> write_promise;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([&write_promise]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(kBasicRequest, _))
            .WillOnce(
                [&write_promise]() { return write_promise.get_future(); });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(absl::optional<FakeResponse>());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(
              absl::make_optional(kBasicResponse));
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(Status());
        });
        return stream;
      });

  ExpectStandardSingleRetryPolicy(mock);

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto write = stream->Write(kBasicRequest,
                             grpc::WriteOptions().set_last_message());
  auto response0_fut = stream->Read();
  write_promise.set_value(true);
  ASSERT_FALSE(response0_fut.get().has_value());
  ASSERT_TRUE(write.get());
  auto response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  auto finish = stream->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, WriteFailWhileReadInFlight) {
  StrictMock<MockStub> mock;
  promise<absl::optional<FakeResponse>> read_promise;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([&read_promise]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([&read_promise]() {
          return read_promise.get_future();
        });
        EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(Status());
        });
        return stream;
      });

  ExpectStandardSingleRetryPolicy(mock);

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto read = stream->Read();
  auto write = stream->Write(kBasicRequest,
                             grpc::WriteOptions().set_last_message());
  read_promise.set_value(absl::make_optional(kBasicResponse));
  auto read_response = read.get();
  ASSERT_TRUE(read_response.has_value());
  EXPECT_EQ(*read_response, kBasicResponse);
  ASSERT_FALSE(write.get());
  write = stream->Write(kBasicRequest,
                        grpc::WriteOptions().set_last_message());
  ASSERT_TRUE(write.get());
  auto finish = stream->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, StartFailsDuringRetryPermanentError) {
  StrictMock<MockStub> mock;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([]() {
          return make_ready_future(absl::optional<FakeResponse>());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      })
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      });

  EXPECT_CALL(mock, FakeRetryPolicy)
      .WillOnce(
          []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); })
      .WillOnce([]() {
        auto mock_retry_policy =
            absl::make_unique<StrictMock<MockRetryPolicy>>();
        EXPECT_CALL(*mock_retry_policy, IsExhausted)
            .WillOnce([]() { return false; })
            .WillOnce([]() { return true; });
        EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillOnce([]() {
          return true;
        });
        return mock_retry_policy;
      });

  INIT_BASIC_STREAM

  auto start = stream->Start();
  ASSERT_FALSE(stream->Read().get().has_value());
  auto finish = stream->Finish();
  finish.get();
  EXPECT_EQ(start.get(), kFailStatus);
}

TEST(AsyncReadWriteStreamingRpcTest, ReadInMiddleOfRetryAfterStart) {
  StrictMock<MockStub> mock;
  promise<bool> start_promise;
  EXPECT_CALL(mock, FakeStream)
      .WillOnce([]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([]() {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce([]() {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(kFailStatus);
        });
        return stream;
      })
      .WillOnce([&start_promise]() {
        auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
        EXPECT_CALL(*stream, Start).WillOnce([&start_promise]() {
          return start_promise.get_future();
        });
        EXPECT_CALL(*stream, Finish).WillOnce([]() {
          return make_ready_future(Status());
        });
        return stream;
      });

  ExpectStandardSingleRetryPolicy(mock);

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto write = stream->Write(kBasicRequest,
                             grpc::WriteOptions().set_last_message());
  auto read = stream->Read();
  start_promise.set_value(true);
  stream->Finish().get();
  ASSERT_FALSE(write.get());
  ASSERT_FALSE(read.get().has_value());
  EXPECT_THAT(start.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, WriteFinishesAfterShutdown) {
  StrictMock<MockStub> mock;
  promise<bool> write_promise;
  EXPECT_CALL(mock, FakeStream).WillOnce([&write_promise]() {
    auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(kBasicRequest, _))
        .WillOnce([&write_promise]() { return write_promise.get_future(); });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(Status());
    });
    return stream;
  });

  EXPECT_CALL(mock, FakeRetryPolicy).WillOnce([]() {
    return absl::make_unique<StrictMock<MockRetryPolicy>>();
  });

  INIT_BASIC_STREAM

  auto start = stream->Start();
  auto write = stream->Write(kBasicRequest,
                             grpc::WriteOptions().set_last_message());
  auto finish = stream->Finish();
  write_promise.set_value(true);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
