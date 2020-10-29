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

#include "google/cloud/bigtable/internal/async_bulk_apply.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = google::bigtable::v2;

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::bigtable::testing::MockClientAsyncReaderInterface;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;

class AsyncBulkApplyTest : public bigtable::testing::TableTestFixture {
 protected:
  AsyncBulkApplyTest()
      : rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        rpc_backoff_policy_(bigtable::DefaultRPCBackoffPolicy(
            internal::kBigtableTableAdminLimits)),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()),
        metadata_update_policy_("my_tqble", MetadataParamTypes::NAME),
        cq_impl_(new FakeCompletionQueueImpl),
        cq_(cq_impl_),
        client_(new testing::MockDataClient) {}

  std::shared_ptr<RPCRetryPolicy const> rpc_retry_policy_;
  std::shared_ptr<RPCBackoffPolicy const> rpc_backoff_policy_;
  std::shared_ptr<IdempotentMutationPolicy> idempotent_mutation_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<FakeCompletionQueueImpl> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<testing::MockDataClient> client_;
};

TEST_F(AsyncBulkApplyTest, AsyncBulkApplySuccess) {
  bigtable::BulkMutation mut{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };

  auto* reader =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  EXPECT_CALL(*reader, Read)
      .WillOnce([](btproto::MutateRowsResponse* r, void*) {
        auto& r1 = *r->add_entries();
        r1.set_index(0);
        r1.mutable_status()->set_code(grpc::StatusCode::OK);

        auto& r2 = *r->add_entries();
        r2.set_index(1);
        r2.mutable_status()->set_code(grpc::StatusCode::OK);
      })
      .WillOnce([](btproto::MutateRowsResponse*, void*) {});

  EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  EXPECT_CALL(*reader, StartCall).Times(1);

  EXPECT_CALL(*client_, PrepareAsyncMutateRows)
      .WillOnce([reader](grpc::ClientContext*,
                         btproto::MutateRowsRequest const&,
                         grpc::CompletionQueue*) {
        return std::unique_ptr<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
            reader);
      })
      .RetiresOnSaturation();

  auto bulk_apply_future = internal::AsyncRetryBulkApply::Create(
      cq_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      *idempotent_mutation_policy_, metadata_update_policy_, client_,
      "my-app-profile", "my-table", std::move(mut));

  bulk_apply_future.then(
      [](future<std::vector<FailedMutation>> f) { f.get(); });

  ASSERT_EQ(1U, cq_impl_->size());

  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING
  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING, 1 read
  cq_impl_->SimulateCompletion(false);
  // state == FINISHING
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(0U, cq_impl_->size());
}

TEST_F(AsyncBulkApplyTest, AsyncBulkApplyPartialSuccessRetry) {
  bigtable::BulkMutation mut{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };

  auto* reader0 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  auto* reader1 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;

  EXPECT_CALL(*reader0, Read)
      .WillOnce([](btproto::MutateRowsResponse* r, void*) {
        auto& r1 = *r->add_entries();
        r1.set_index(0);
        r1.mutable_status()->set_code(grpc::StatusCode::OK);
      })
      .WillOnce([](btproto::MutateRowsResponse*, void*) {});

  EXPECT_CALL(*reader1, Read)
      .WillOnce([](btproto::MutateRowsResponse* r, void*) {
        auto& r1 = *r->add_entries();
        r1.set_index(0);
        r1.mutable_status()->set_code(grpc::StatusCode::OK);
      })
      .WillOnce([](btproto::MutateRowsResponse*, void*) {});

  EXPECT_CALL(*reader0, Finish).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  EXPECT_CALL(*reader1, Finish).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  EXPECT_CALL(*reader0, StartCall).Times(1);
  EXPECT_CALL(*reader1, StartCall).Times(1);

  EXPECT_CALL(*client_, PrepareAsyncMutateRows)
      .WillOnce([reader0](grpc::ClientContext*,
                          btproto::MutateRowsRequest const&,
                          grpc::CompletionQueue*) {
        return std::unique_ptr<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
            reader0);
      })
      .WillOnce([reader1](grpc::ClientContext*,
                          btproto::MutateRowsRequest const&,
                          grpc::CompletionQueue*) {
        return std::unique_ptr<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
            reader1);
      });

  auto bulk_apply_future = internal::AsyncRetryBulkApply::Create(
      cq_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      *idempotent_mutation_policy_, metadata_update_policy_, client_,
      "my-app-profile", "my-table", std::move(mut));

  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING
  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING, 1 read
  cq_impl_->SimulateCompletion(false);
  // state == FINISHING
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING
  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING, 1 read
  cq_impl_->SimulateCompletion(false);
  // state == FINISHING
  cq_impl_->SimulateCompletion(true);

  bulk_apply_future.get();

  ASSERT_EQ(0U, cq_impl_->size());
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncBulkApplyTest, AsyncBulkApplyFailureRetry) {
  bigtable::BulkMutation mut{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };

  auto* reader0 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  auto* reader1 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;

  EXPECT_CALL(*reader0, Read)
      .WillOnce([](btproto::MutateRowsResponse* r, void*) {
        auto& r1 = *r->add_entries();
        r1.set_index(0);
        r1.mutable_status()->set_code(grpc::StatusCode::OK);
      })
      .WillOnce([](btproto::MutateRowsResponse*, void*) {});

  EXPECT_CALL(*reader1, Read)
      .WillOnce([](btproto::MutateRowsResponse* r, void*) {
        auto& r1 = *r->add_entries();
        r1.set_index(0);
        r1.mutable_status()->set_code(grpc::StatusCode::OK);

        auto& r2 = *r->add_entries();
        r2.set_index(1);
        r2.mutable_status()->set_code(grpc::StatusCode::OK);
      })
      .WillOnce([](btproto::MutateRowsResponse*, void*) {});

  EXPECT_CALL(*reader0, Finish).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "");
  });

  EXPECT_CALL(*reader1, Finish).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  EXPECT_CALL(*reader0, StartCall).Times(1);
  EXPECT_CALL(*reader1, StartCall).Times(1);

  EXPECT_CALL(*client_, PrepareAsyncMutateRows)
      .WillOnce([reader0](grpc::ClientContext*,
                          btproto::MutateRowsRequest const&,
                          grpc::CompletionQueue*) {
        return std::unique_ptr<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
            reader0);
      })
      .WillOnce([reader1](grpc::ClientContext*,
                          btproto::MutateRowsRequest const&,
                          grpc::CompletionQueue*) {
        return std::unique_ptr<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
            reader1);
      });

  auto bulk_apply_future = internal::AsyncRetryBulkApply::Create(
      cq_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      *idempotent_mutation_policy_, metadata_update_policy_, client_,
      "my-app-profile", "my-table", std::move(mut));

  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING
  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING, 1 read
  cq_impl_->SimulateCompletion(false);
  // state == FINISHING
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING
  cq_impl_->SimulateCompletion(true);
  // state == PROCESSING, 1 read
  cq_impl_->SimulateCompletion(false);
  // state == FINISHING
  cq_impl_->SimulateCompletion(true);

  bulk_apply_future.get();

  ASSERT_EQ(0U, cq_impl_->size());

  EXPECT_TRUE(cq_impl_->empty());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
