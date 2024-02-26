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

#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/common_options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;

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

using MockReturnType = std::unique_ptr<
    grpc::ClientAsyncReaderWriterInterface<FakeRequest, FakeResponse>>;

class MockReaderWriter
    : public grpc::ClientAsyncReaderWriterInterface<FakeRequest, FakeResponse> {
 public:
  MOCK_METHOD(void, WritesDone, (void*), (override));
  MOCK_METHOD(void, Read, (FakeResponse*, void*), (override));
  MOCK_METHOD(void, Write, (FakeRequest const&, void*), (override));
  MOCK_METHOD(void, Write, (FakeRequest const&, grpc::WriteOptions, void*),
              (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
};

class MockStub {
 public:
  MOCK_METHOD(MockReturnType, FakeRpc,
              (grpc::ClientContext*, grpc::CompletionQueue*), ());
};

TEST(AsyncReadWriteStreamingRpcTest, Basic) {
  MockStub mock;
  EXPECT_CALL(mock, FakeRpc)
      .WillOnce([](grpc::ClientContext*, grpc::CompletionQueue*) {
        auto stream = std::make_unique<MockReaderWriter>();
        EXPECT_CALL(*stream, StartCall).Times(1);
        EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _, _)).Times(1);
        EXPECT_CALL(*stream, Read)
            .WillOnce([](FakeResponse* response, void*) {
              response->key = "key0";
              response->value = "value0_0";
            })
            .WillOnce([](FakeResponse* response, void*) {
              response->key = "key0";
              response->value = "value0_1";
            })
            .WillOnce([](FakeResponse*, void*) {});
        EXPECT_CALL(*stream, WritesDone).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(Return(nullptr));

  std::deque<std::shared_ptr<AsyncGrpcOperation>> operations;
  auto notify_next_op = [&](bool ok = true) {
    auto op = std::move(operations.front());
    operations.pop_front();
    op->Notify(ok);
  };

  EXPECT_CALL(*mock_cq, StartOperation)
      .WillRepeatedly([&operations](std::shared_ptr<AsyncGrpcOperation> op,
                                    absl::FunctionRef<void(void*)> call) {
        void* tag = op.get();
        operations.push_back(std::move(op));
        call(tag);
      });

  google::cloud::CompletionQueue cq(mock_cq);
  OptionsSpan create_span(Options{}.set<UserProjectOption>("unused"));
  auto stream = MakeStreamingReadWriteRpc<FakeRequest, FakeResponse>(
      cq, std::make_shared<grpc::ClientContext>(),
      MakeImmutableOptions(Options{}.set<UserProjectOption>("create")),
      [&mock](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
        return mock.FakeRpc(context, cq);
      });

  OptionsSpan start_span(Options{}.set<UserProjectOption>("start"));
  auto start = stream->Start().then([](auto f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "create");
    return f.get();
  });
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  EXPECT_TRUE(start.get());

  OptionsSpan write_span(Options{}.set<UserProjectOption>("write"));
  auto write =
      stream
          ->Write(FakeRequest{"key0"}, grpc::WriteOptions().set_last_message())
          .then([](auto f) {
            EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "create");
            return f.get();
          });
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  EXPECT_TRUE(write.get());

  OptionsSpan read0_span(Options{}.set<UserProjectOption>("read0"));
  auto read0 = stream->Read().then([](auto f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "create");
    return f.get();
  });
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  auto response0 = read0.get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ("key0", response0->key);
  EXPECT_EQ("value0_0", response0->value);

  OptionsSpan read1_span(Options{}.set<UserProjectOption>("read1"));
  auto read1 = stream->Read().then([](auto f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "create");
    return f.get();
  });
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  auto response1 = read1.get();
  ASSERT_TRUE(response1.has_value());
  EXPECT_EQ("key0", response1->key);
  EXPECT_EQ("value0_1", response1->value);

  OptionsSpan writes_done_span(Options{}.set<UserProjectOption>("writes_done"));
  auto writes_done = stream->WritesDone().then([](auto f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "create");
    return f.get();
  });
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  EXPECT_TRUE(writes_done.get());

  OptionsSpan read2_span(Options{}.set<UserProjectOption>("read2"));
  auto read2 = stream->Read().then([](auto f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "create");
    return f.get();
  });
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op(false);
  auto response2 = read2.get();
  EXPECT_FALSE(response2.has_value());

  OptionsSpan finish_span(Options{}.set<UserProjectOption>("finish"));
  auto finish = stream->Finish().then([](auto f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "create");
    return f.get();
  });
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  EXPECT_THAT(finish.get(), IsOk());
}

TEST(AsyncReadWriteStreamingRpcTest, Error) {
  auto stream = AsyncStreamingReadWriteRpcError<FakeRequest, FakeResponse>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));

  stream.Cancel();
  EXPECT_FALSE(stream.Start().get());
  EXPECT_FALSE(stream.Read().get().has_value());
  EXPECT_FALSE(stream.Write(FakeRequest{}, grpc::WriteOptions()).get());
  EXPECT_FALSE(stream.WritesDone().get());
  EXPECT_EQ(Status(StatusCode::kPermissionDenied, "uh-oh"),
            stream.Finish().get());
  auto const metadata = stream.GetRequestMetadata();
  EXPECT_THAT(metadata.headers, IsEmpty());
  EXPECT_THAT(metadata.trailers, IsEmpty());
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

using ::google::cloud::testing_util::IsActive;

TEST(AsyncReadWriteStreamingRpcTest, SpanActiveAcrossAsyncGrpcOperations) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockStub mock;
  EXPECT_CALL(mock, FakeRpc)
      .WillOnce([](grpc::ClientContext*, grpc::CompletionQueue*) {
        auto stream = std::make_unique<MockReaderWriter>();
        EXPECT_CALL(*stream, StartCall).Times(1);
        EXPECT_CALL(*stream, Write(_, _, _)).Times(1);
        EXPECT_CALL(*stream, Read).Times(1);
        EXPECT_CALL(*stream, WritesDone).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(Return(nullptr));

  std::deque<std::shared_ptr<AsyncGrpcOperation>> operations;
  auto notify_next_op = [&](bool ok = true) {
    auto op = std::move(operations.front());
    operations.pop_front();
    op->Notify(ok);
  };

  EXPECT_CALL(*mock_cq, StartOperation)
      .WillRepeatedly([&operations](std::shared_ptr<AsyncGrpcOperation> op,
                                    absl::FunctionRef<void(void*)> call) {
        void* tag = op.get();
        operations.push_back(std::move(op));
        call(tag);
      });

  google::cloud::CompletionQueue cq(mock_cq);

  auto stream = [&] {
    auto span = MakeSpan("create");
    OTelScope scope(span);
    return MakeStreamingReadWriteRpc<FakeRequest, FakeResponse>(
        cq, std::make_shared<grpc::ClientContext>(), MakeImmutableOptions({}),
        [&mock, span](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
          EXPECT_THAT(span, IsActive());
          return mock.FakeRpc(context, cq);
        });
  }();

  auto start = [&] {
    auto span = MakeSpan("start");
    OTelScope scope(span);
    return stream->Start().then([span](auto f) {
      EXPECT_THAT(span, IsActive());
      return f.get();
    });
  }();
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  (void)start.get();

  auto write = [&] {
    auto span = MakeSpan("start");
    OTelScope scope(span);
    return stream
        ->Write(FakeRequest{"key0"}, grpc::WriteOptions().set_last_message())
        .then([span](auto f) {
          EXPECT_THAT(span, IsActive());
          return f.get();
        });
  }();
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  (void)write.get();

  auto read = [&] {
    auto span = MakeSpan("read");
    OTelScope scope(span);
    return stream->Read().then([span](auto f) {
      EXPECT_THAT(span, IsActive());
      return f.get();
    });
  }();
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  (void)read.get();

  auto writes_done = [&] {
    auto span = MakeSpan("start");
    OTelScope scope(span);
    return stream->WritesDone().then([span](auto f) {
      EXPECT_THAT(span, IsActive());
      return f.get();
    });
  }();
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  (void)writes_done.get();

  auto finish = [&] {
    auto span = MakeSpan("finish");
    OTelScope scope(span);
    return stream->Finish().then([span](auto f) {
      EXPECT_THAT(span, IsActive());
      return f.get();
    });
  }();
  ASSERT_THAT(operations, SizeIs(1));
  notify_next_op();
  (void)finish.get();
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
