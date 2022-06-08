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

#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/common_options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
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
using ::testing::ReturnRef;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string key;
  std::string value;
};

using MockReturnType =
    std::unique_ptr<grpc::ClientAsyncWriterInterface<FakeRequest>>;

class MockWriter : public grpc::ClientAsyncWriterInterface<FakeRequest> {
 public:
  MOCK_METHOD(void, Write, (FakeRequest const&, void*), (override));
  MOCK_METHOD(void, Write, (FakeRequest const&, grpc::WriteOptions, void*),
              (override));
  MOCK_METHOD(void, WritesDone, (void*), (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
};

class MockStub {
 public:
  MOCK_METHOD(MockReturnType, FakeRpc,
              (grpc::ClientContext*, FakeResponse*, grpc::CompletionQueue*),
              ());
};

TEST(AsyncStreamingWriteRpcTest, Basic) {
  MockStub mock;
  EXPECT_CALL(mock, FakeRpc)
      .WillOnce([](grpc::ClientContext*, FakeResponse* response,
                   grpc::CompletionQueue*) {
        auto stream = absl::make_unique<MockWriter>();
        EXPECT_CALL(*stream, StartCall).Times(1);
        // Need to specify that it takes 3 parameters to disambiguate which
        // Write() call to expect.
        EXPECT_CALL(*stream, Write(_, _, _)).Times(2);
        EXPECT_CALL(*stream, WritesDone).Times(1);
        EXPECT_CALL(*stream, Finish)
            .WillOnce([response](grpc::Status* status, void*) {
              response->key = "key0";
              response->value = "value0_0";
              *status = grpc::Status::OK;
            });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  grpc::CompletionQueue grpc_cq;
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(ReturnRef(grpc_cq));

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

  auto user_project = [](std::string const& value) {
    return Options{}.set<UserProjectOption>(value);
  };
  auto check_write_span = [](std::string const& expected) {
    return [expected](future<bool> f) {
      EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), expected);
      return f.get();
    };
  };

  google::cloud::CompletionQueue cq(mock_cq);

  OptionsSpan span(user_project("create"));
  auto stream = MakeStreamingWriteRpc<FakeRequest, FakeResponse>(
      cq, absl::make_unique<grpc::ClientContext>(),
      [&mock](grpc::ClientContext* context, FakeResponse* response,
              grpc::CompletionQueue* cq) {
        return mock.FakeRpc(context, response, cq);
      });

  OptionsSpan start_span(user_project("start"));
  auto start = stream->Start().then([](future<bool> f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "start");
    return f.get();
  });
  ASSERT_EQ(1, operations.size());
  OptionsSpan start_clear(Options{});
  notify_next_op();
  EXPECT_TRUE(start.get());

  OptionsSpan write0_span(user_project("write0"));
  auto write0 = stream->Write(FakeRequest{}, grpc::WriteOptions())
                    .then(check_write_span("write0"));
  ASSERT_EQ(1, operations.size());
  OptionsSpan write0_clear(Options{});
  notify_next_op();
  ASSERT_TRUE(write0.get());

  OptionsSpan write1_span(user_project("write1"));
  auto write1 = stream->Write(FakeRequest{}, grpc::WriteOptions())
                    .then(check_write_span("write1"));
  ASSERT_EQ(1, operations.size());
  OptionsSpan write1_clear(Options{});
  notify_next_op(false);
  EXPECT_FALSE(write1.get());

  OptionsSpan writes_done_span(user_project("writes_done"));
  auto writes_done = stream->WritesDone().then(check_write_span("writes_done"));
  ASSERT_EQ(1, operations.size());
  OptionsSpan writes_done_clear(Options{});
  notify_next_op(false);
  EXPECT_FALSE(writes_done.get());

  OptionsSpan finish_span(user_project("finish"));
  auto finish = stream->Finish().then([](future<StatusOr<FakeResponse>> f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "finish");
    return f.get();
  });
  ASSERT_EQ(1, operations.size());
  OptionsSpan finish_clear(Options{});
  notify_next_op();
  auto response = finish.get();
  ASSERT_THAT(response, IsOk());
  EXPECT_EQ(response->key, "key0");
  EXPECT_EQ(response->value, "value0_0");
}

TEST(AsyncStreamingWriteRpcTest, Error) {
  auto stream = AsyncStreamingWriteRpcError<FakeRequest, FakeResponse>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));

  stream.Cancel();
  EXPECT_FALSE(stream.Start().get());
  EXPECT_FALSE(stream.Write(FakeRequest{}, grpc::WriteOptions()).get());
  EXPECT_EQ(Status(StatusCode::kPermissionDenied, "uh-oh"),
            stream.Finish().get().status());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
