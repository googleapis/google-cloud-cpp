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

#include "google/cloud/bigtable/internal/async_row_sampler.h"
#include "google/cloud/bigtable/testing/mock_sample_row_keys_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/bigtable/v2/bigtable.pb.h"
#include "rpc_policy_parameters.h"
#include <gmock/gmock.h>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = ::google::bigtable::v2;

using ::google::cloud::bigtable::testing::MockClientAsyncReaderInterface;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;

class AsyncSampleRowKeysTest : public bigtable::testing::TableTestFixture {
 protected:
  AsyncSampleRowKeysTest()
      : TableTestFixture(
            CompletionQueue(std::make_shared<FakeCompletionQueueImpl>())),
        rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        metadata_update_policy_("my_table", MetadataParamTypes::NAME) {}

  void SimulateIteration() {
    // Finish Start()
    cq_impl_->SimulateCompletion(true);
    // Return data
    cq_impl_->SimulateCompletion(true);
    // Finish stream
    cq_impl_->SimulateCompletion(false);
    // Finish Finish()
    cq_impl_->SimulateCompletion(true);
  }

  std::shared_ptr<RPCRetryPolicy const> rpc_retry_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
};

TEST_F(AsyncSampleRowKeysTest, Simple) {
  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
    .WillOnce([](grpc::ClientContext*,
          btproto::SampleRowKeysRequest const&,
          grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read)
            .WillOnce([](btproto::SampleRowKeysResponse* r, void*) {
              {
                r->set_row_key("test1");
                r->set_offset_bytes(11);
              }
            })
            .WillOnce([](btproto::SampleRowKeysResponse*, void*) {});

        EXPECT_CALL(*reader, Finish)
            .WillOnce([](grpc::Status* status, void*) {
                *status = grpc::Status::OK;
                });
        return reader;
        });

  auto samples_future = table_.AsyncSampleRows();

  SimulateIteration();

  auto status = samples_future.get();
  ASSERT_STATUS_OK(status);

  auto samples = status.value();
  ASSERT_EQ(1U, samples.size());
  EXPECT_EQ("test1", samples[0].row_key);
  EXPECT_EQ(11, samples[0].offset_bytes);
}

TEST_F(AsyncSampleRowKeysTest, Retry) {
  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
    .WillOnce([](grpc::ClientContext*,
          btproto::SampleRowKeysRequest const&,
          grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read)
            .WillOnce([](btproto::SampleRowKeysResponse* r, void*) {
              {
                r->set_row_key("test1");
                r->set_offset_bytes(11);
              }
            })
            .WillOnce([](btproto::SampleRowKeysResponse*, void*) {});

        EXPECT_CALL(*reader, Finish)
            .WillOnce([](grpc::Status* status, void*) {
                *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
                });
            return reader;
            })
    .WillOnce([](grpc::ClientContext*,
          btproto::SampleRowKeysRequest const&,
          grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read)
            .WillOnce([](btproto::SampleRowKeysResponse* r, void*) {
              {
                r->set_row_key("test2");
                r->set_offset_bytes(22);
              }
            })
            .WillOnce([](btproto::SampleRowKeysResponse*, void*) {});
        EXPECT_CALL(*reader, Finish)
            .WillOnce([](grpc::Status* status, void*) {
                *status = grpc::Status::OK;
                });
        return reader;
        });

  auto samples_future = table_.AsyncSampleRows();

  SimulateIteration();
  // simulate the backoff timer
  cq_impl_->SimulateCompletion(true);
  ASSERT_EQ(1U, cq_impl_->size());

  SimulateIteration();
  ASSERT_EQ(0U, cq_impl_->size());

  auto status = samples_future.get();
  ASSERT_STATUS_OK(status);

  auto samples = status.value();
  ASSERT_EQ(1U, samples.size());
  EXPECT_EQ("test2", samples[0].row_key);
  EXPECT_EQ(22, samples[0].offset_bytes);
}

TEST_F(AsyncSampleRowKeysTest, TooManyFailures) {
  // We give up on the 3rd error.
  auto constexpr kErrorCount = 2;
  Table custom_table(client_, "foo_table",
      LimitedErrorCountRetryPolicy(kErrorCount));

  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
    .Times(kErrorCount + 1)
    .WillRepeatedly([](grpc::ClientContext*,
          btproto::SampleRowKeysRequest const&,
          grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish)
            .WillOnce([](grpc::Status* status, void*) {
                *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
                });
            return reader;
        });

  auto samples_future = custom_table.AsyncSampleRows();

  for (int retry = 0; retry < kErrorCount; ++retry) {
    SimulateIteration();
    // simulate the backoff timer
    cq_impl_->SimulateCompletion(true);
    ASSERT_EQ(1U, cq_impl_->size());
  }

  SimulateIteration();

  auto status = samples_future.get();
  ASSERT_THAT(status, StatusIs(StatusCode::kUnavailable));
  
  ASSERT_EQ(0U, cq_impl_->size());
}

class MockBackoffPolicy : public RPCBackoffPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<RPCBackoffPolicy>, clone, (), (const, override));
  MOCK_METHOD(void, Setup, (grpc::ClientContext & context), (const, override));
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion, (Status const& status),
              (override));
  // TODO(#2344) - remove ::grpc::Status version.
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion,
              (grpc::Status const& status), (override));
};

TEST_F(AsyncSampleRowKeysTest, UsesBackoff) {
  auto grpc_error = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
  auto error = MakeStatusFromRpcError(grpc_error);

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, Setup).Times(2);
  EXPECT_CALL(*mock, OnCompletion(error));

  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
    .WillOnce([grpc_error](grpc::ClientContext*,
          btproto::SampleRowKeysRequest const&,
          grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish)
            .WillOnce([grpc_error](grpc::Status* status, void*) {
                *status = grpc_error;
                });
            return reader;
            })
    .WillOnce([](grpc::ClientContext*,
          btproto::SampleRowKeysRequest const&,
          grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish)
            .WillOnce([](grpc::Status* status, void*) {
                *status = grpc::Status::OK;
                });
        return reader;
        });

  auto samples_future = internal::AsyncRowSampler::Create(cq_, client_,
     rpc_retry_policy_->clone(), std::move(mock),
     metadata_update_policy_,
     "my-app-profile", "my-table");

  SimulateIteration();
  // simulate the backoff timer
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  SimulateIteration();
  
  ASSERT_EQ(0U, cq_impl_->size());
}

TEST_F(AsyncSampleRowKeysTest, CancelDuringBackoff) {
  auto grpc_error = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
  auto error = MakeStatusFromRpcError(grpc_error);

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, Setup);
  EXPECT_CALL(*mock, OnCompletion(error));

  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
    .WillOnce([grpc_error](grpc::ClientContext*,
          btproto::SampleRowKeysRequest const&,
          grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish)
            .WillOnce([grpc_error](grpc::Status* status, void*) {
                *status = grpc_error;
                });
            return reader;
            });

  auto samples_future = internal::AsyncRowSampler::Create(cq_, client_,
     rpc_retry_policy_->clone(), std::move(mock),
     metadata_update_policy_,
     "my-app-profile", "my-table");

  SimulateIteration();
  ASSERT_EQ(1U, cq_impl_->size());

  // cancel the pending operation.
  samples_future.cancel();
  // simulate the backoff timer expiring
  cq_impl_->SimulateCompletion(false);

  ASSERT_EQ(0U, cq_impl_->size());

  auto status = samples_future.get();
  ASSERT_THAT(status, StatusIs(StatusCode::kCancelled));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
