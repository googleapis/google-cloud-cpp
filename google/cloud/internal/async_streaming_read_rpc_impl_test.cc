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

#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
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
using ::testing::ReturnRef;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string key;
  std::string value;
};

using MockReturnType =
    std::unique_ptr<grpc::ClientAsyncReaderInterface<FakeResponse>>;

class MockReader : public grpc::ClientAsyncReaderInterface<FakeResponse> {
 public:
  MOCK_METHOD(void, Read, (FakeResponse*, void*), (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
};

class MockStub {
 public:
  MOCK_METHOD(MockReturnType, FakeRpc,
              (grpc::ClientContext*, FakeRequest const&,
               grpc::CompletionQueue*),
              ());
};

TEST(AsyncStreamingReadRpcTest, Basic) {
  MockStub mock;
  EXPECT_CALL(mock, FakeRpc)
      .WillOnce([](grpc::ClientContext*, FakeRequest const&,
                   grpc::CompletionQueue*) {
        auto stream = absl::make_unique<MockReader>();
        EXPECT_CALL(*stream, StartCall).Times(1);
        EXPECT_CALL(*stream, Read)
            .WillOnce([](FakeResponse* response, void*) {
              response->key = "key0";
              response->value = "value0_0";
            })
            .WillOnce([](FakeResponse*, void*) {});
        EXPECT_CALL(*stream, Finish).WillOnce([](grpc::Status* status, void*) {
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

  auto set_span = [](std::string const& value) {
    return OptionsSpan(Options{}.set<UserProjectOption>(value));
  };
  auto clear_span = []() { return OptionsSpan(Options{}); };
  auto check_read_span = [](std::string const& expected) {
    return [expected](future<absl::optional<FakeResponse>> f) {
      EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), expected);
      return f.get();
    };
  };

  google::cloud::CompletionQueue cq(mock_cq);

  auto span = set_span("create");
  auto stream = MakeStreamingReadRpc<FakeRequest, FakeResponse>(
      cq, absl::make_unique<grpc::ClientContext>(), FakeRequest{},
      [&mock](grpc::ClientContext* context, FakeRequest const& request,
              grpc::CompletionQueue* cq) {
        return mock.FakeRpc(context, request, cq);
      });

  auto start_span = set_span("start");
  auto start = stream->Start().then([](future<bool> f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "start");
    return f.get();
  });
  ASSERT_EQ(1, operations.size());
  auto start_clear = clear_span();
  notify_next_op();
  EXPECT_TRUE(start.get());

  auto read0_span = set_span("read0");
  auto read0 = stream->Read().then(check_read_span("read0"));
  ASSERT_EQ(1, operations.size());
  auto read0_clear = clear_span();
  notify_next_op();
  auto response0 = read0.get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ("key0", response0->key);
  EXPECT_EQ("value0_0", response0->value);

  auto read1_span = set_span("read1");
  auto read1 = stream->Read().then(check_read_span("read1"));
  ASSERT_EQ(1, operations.size());
  auto read1_clear = clear_span();
  notify_next_op(false);
  auto response1 = read1.get();
  EXPECT_FALSE(response1.has_value());

  auto finish_span = set_span("finish");
  auto finish = stream->Finish().then([](future<Status> f) {
    EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "finish");
    return f.get();
  });
  ASSERT_EQ(1, operations.size());
  auto finish_clear = clear_span();
  notify_next_op();
  EXPECT_THAT(finish.get(), IsOk());
}

TEST(AsyncStreamingReadRpcTest, Error) {
  auto stream = AsyncStreamingReadRpcError<FakeResponse>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));

  stream.Cancel();
  EXPECT_FALSE(stream.Start().get());
  EXPECT_FALSE(stream.Read().get().has_value());
  EXPECT_EQ(Status(StatusCode::kPermissionDenied, "uh-oh"),
            stream.Finish().get());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
