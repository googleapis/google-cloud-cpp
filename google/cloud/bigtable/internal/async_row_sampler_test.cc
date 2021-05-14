// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/internal/async_row_sampler.h"
#include "google/cloud/bigtable/testing/mock_backoff_policy.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <gmock/gmock.h>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = ::google::bigtable::v2;

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockClientAsyncReaderInterface;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

class AsyncSampleRowKeysTest : public bigtable::testing::TableTestFixture {
 protected:
  AsyncSampleRowKeysTest()
      : TableTestFixture(
            CompletionQueue(std::make_shared<FakeCompletionQueueImpl>())),
        rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        metadata_update_policy_("my_table", MetadataParamTypes::NAME) {}

  std::shared_ptr<RPCRetryPolicy const> rpc_retry_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
};

struct RowKeySampleVectors {
  explicit RowKeySampleVectors(std::vector<RowKeySample> samples) {
    row_keys.reserve(samples.size());
    offset_bytes.reserve(samples.size());
    for (auto& sample : samples) {
      row_keys.emplace_back(std::move(sample.row_key));
      offset_bytes.emplace_back(std::move(sample.offset_bytes));
    }
  }

  std::vector<std::string> row_keys;
  std::vector<std::int64_t> offset_bytes;
};

TEST_F(AsyncSampleRowKeysTest, Simple) {
  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
      .WillOnce([](grpc::ClientContext*, btproto::SampleRowKeysRequest const&,
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
            .WillOnce([](btproto::SampleRowKeysResponse* r, void*) {
              {
                r->set_row_key("test2");
                r->set_offset_bytes(22);
              }
            })
            .WillOnce([](btproto::SampleRowKeysResponse*, void*) {});

        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return reader;
      });

  auto samples_future = table_.AsyncSampleRows();

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response 1
  cq_impl_->SimulateCompletion(true);
  // Return response 2
  cq_impl_->SimulateCompletion(true);
  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);

  auto status = samples_future.get();
  ASSERT_STATUS_OK(status);

  auto samples = RowKeySampleVectors(status.value());
  EXPECT_THAT(samples.row_keys, ElementsAre("test1", "test2"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(11, 22));
}

TEST_F(AsyncSampleRowKeysTest, Retry) {
  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
      .WillOnce([](grpc::ClientContext*, btproto::SampleRowKeysRequest const&,
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

        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
        });
        return reader;
      })
      .WillOnce([](grpc::ClientContext*, btproto::SampleRowKeysRequest const&,
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
        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return reader;
      });

  auto samples_future = table_.AsyncSampleRows();

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response
  cq_impl_->SimulateCompletion(true);
  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);
  // Simulate the backoff timer
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response
  cq_impl_->SimulateCompletion(true);
  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(0U, cq_impl_->size());

  auto status = samples_future.get();
  ASSERT_STATUS_OK(status);

  auto samples = RowKeySampleVectors(status.value());
  EXPECT_THAT(samples.row_keys, ElementsAre("test2"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(22));
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
        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
        });
        return reader;
      });

  auto samples_future = custom_table.AsyncSampleRows();

  for (int retry = 0; retry < kErrorCount; ++retry) {
    // Start()
    cq_impl_->SimulateCompletion(true);
    // Return response
    cq_impl_->SimulateCompletion(true);
    // End stream
    cq_impl_->SimulateCompletion(false);
    // Finish()
    cq_impl_->SimulateCompletion(true);
    // Simulate the backoff timer
    cq_impl_->SimulateCompletion(true);

    ASSERT_EQ(1U, cq_impl_->size());
  }

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response
  cq_impl_->SimulateCompletion(true);
  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);

  auto status = samples_future.get();
  ASSERT_THAT(status,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try again")));

  ASSERT_EQ(0U, cq_impl_->size());
}

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
      .WillOnce([](grpc::ClientContext*, btproto::SampleRowKeysRequest const&,
                   grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>();
        EXPECT_CALL(*reader, StartCall);
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return reader;
      });

  auto samples_future = internal::AsyncRowSampler::Create(
      cq_, client_, rpc_retry_policy_->clone(), std::move(mock),
      metadata_update_policy_, "my-app-profile", "my-table");

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response
  cq_impl_->SimulateCompletion(true);
  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);
  // Simulate the backoff timer
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response
  cq_impl_->SimulateCompletion(true);
  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);

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

  auto samples_future = internal::AsyncRowSampler::Create(
      cq_, client_, rpc_retry_policy_->clone(), std::move(mock),
      metadata_update_policy_, "my-app-profile", "my-table");

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response
  cq_impl_->SimulateCompletion(true);
  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  // Cancel the pending operation.
  samples_future.cancel();
  // Simulate the backoff timer
  cq_impl_->SimulateCompletion(false);

  ASSERT_EQ(0U, cq_impl_->size());

  auto status = samples_future.get();
  ASSERT_THAT(status,
              StatusIs(StatusCode::kCancelled, HasSubstr("call cancelled")));
}

TEST_F(AsyncSampleRowKeysTest, CancelAfterSuccess) {
  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
      .WillOnce([](grpc::ClientContext*, btproto::SampleRowKeysRequest const&,
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

        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return reader;
      });

  auto samples_future = table_.AsyncSampleRows();
  // samples_future.cancel();

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response
  cq_impl_->SimulateCompletion(true);

  // Cancel the pending operation
  samples_future.cancel();

  // End stream
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);

  auto status = samples_future.get();
  ASSERT_STATUS_OK(status);

  auto samples = RowKeySampleVectors(status.value());
  EXPECT_THAT(samples.row_keys, ElementsAre("test1"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(11));
}

TEST_F(AsyncSampleRowKeysTest, CancelMidStream) {
  EXPECT_CALL(*client_, PrepareAsyncSampleRowKeys)
      .WillOnce([](grpc::ClientContext*, btproto::SampleRowKeysRequest const&,
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
            .WillOnce([](btproto::SampleRowKeysResponse* r, void*) {
              {
                r->set_row_key("test2");
                r->set_offset_bytes(22);
              }
            });
        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status(grpc::StatusCode::CANCELLED, "User cancelled");
        });
        return reader;
      });

  auto samples_future = table_.AsyncSampleRows();

  // Start()
  cq_impl_->SimulateCompletion(true);
  // Return response 1
  cq_impl_->SimulateCompletion(true);
  // Cancel the pending operation
  samples_future.cancel();
  // Return response 2
  cq_impl_->SimulateCompletion(false);
  // Finish()
  cq_impl_->SimulateCompletion(true);

  auto status = samples_future.get();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kCancelled, HasSubstr("User cancelled")));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
