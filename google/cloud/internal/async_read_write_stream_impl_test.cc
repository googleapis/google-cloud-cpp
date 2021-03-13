// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <deque>
#include <memory>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
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
        auto stream = absl::make_unique<MockReaderWriter>();
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

  google::cloud::CompletionQueue cq(mock_cq);
  auto stream = MakeStreamingReadWriteRpc<FakeRequest, FakeResponse>(
      cq, absl::make_unique<grpc::ClientContext>(),
      [&mock](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
        return mock.FakeRpc(context, cq);
      });

  auto start = stream->Start();
  ASSERT_EQ(1, operations.size());
  notify_next_op();
  EXPECT_TRUE(start.get());

  auto write = stream->Write(FakeRequest{"key0"},
                             grpc::WriteOptions().set_last_message());
  ASSERT_EQ(1, operations.size());
  notify_next_op();

  auto read0 = stream->Read();
  ASSERT_EQ(1, operations.size());
  notify_next_op();
  auto response0 = read0.get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ("key0", response0->key);
  EXPECT_EQ("value0_0", response0->value);

  auto read1 = stream->Read();
  ASSERT_EQ(1, operations.size());
  notify_next_op();
  auto response1 = read1.get();
  ASSERT_TRUE(response1.has_value());
  EXPECT_EQ("key0", response1->key);
  EXPECT_EQ("value0_1", response1->value);

  auto writes_done = stream->WritesDone();
  ASSERT_EQ(1, operations.size());
  notify_next_op();

  auto read2 = stream->Read();
  ASSERT_EQ(1, operations.size());
  notify_next_op(false);
  auto response2 = read2.get();
  EXPECT_FALSE(response2.has_value());

  auto finish = stream->Finish();
  ASSERT_EQ(1, operations.size());
  notify_next_op();
  EXPECT_THAT(finish.get(), IsOk());
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
