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
#include "google/cloud/bigtable/testing/mock_backoff_policy.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = google::bigtable::v2;

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockClientAsyncReaderInterface;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;

class AsyncBulkApplyTest : public bigtable::testing::TableTestFixture {
 protected:
  AsyncBulkApplyTest()
      : TableTestFixture(
            CompletionQueue(std::make_shared<FakeCompletionQueueImpl>())),
        rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        rpc_backoff_policy_(bigtable::DefaultRPCBackoffPolicy(
            internal::kBigtableTableAdminLimits)),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()),
        metadata_update_policy_("my_table", MetadataParamTypes::NAME) {}

  void SimulateIteration() {
    cq_impl_->SimulateCompletion(true);
    // state == PROCESSING
    cq_impl_->SimulateCompletion(true);
    // state == PROCESSING, 1 read
    cq_impl_->SimulateCompletion(false);
    // state == FINISHING
    cq_impl_->SimulateCompletion(true);
  }

  std::shared_ptr<RPCRetryPolicy const> rpc_retry_policy_;
  std::shared_ptr<RPCBackoffPolicy const> rpc_backoff_policy_;
  std::shared_ptr<IdempotentMutationPolicy> idempotent_mutation_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
};

std::vector<Status> StatusOnly(std::vector<FailedMutation> const& failures) {
  std::vector<Status> v;
  std::transform(failures.begin(), failures.end(), std::back_inserter(v),
                 [](FailedMutation const& f) { return f.status(); });
  return v;
}

TEST_F(AsyncBulkApplyTest, NoMutations) {
  bigtable::BulkMutation mut;

  auto bulk_apply_future = internal::AsyncRetryBulkApply::Create(
      cq_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      *idempotent_mutation_policy_, metadata_update_policy_, client_,
      "my-app-profile", "my-table", std::move(mut));

  ASSERT_EQ(0U, bulk_apply_future.get().size());
}

TEST_F(AsyncBulkApplyTest, Success) {
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

  SimulateIteration();

  ASSERT_EQ(0U, cq_impl_->size());
}

TEST_F(AsyncBulkApplyTest, PartialSuccessRetry) {
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

  SimulateIteration();
  // simulate the backoff timer
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  SimulateIteration();

  bulk_apply_future.get();

  ASSERT_EQ(0U, cq_impl_->size());
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncBulkApplyTest, DefaultFailureRetry) {
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

  SimulateIteration();
  // simulate the backoff timer
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  SimulateIteration();

  bulk_apply_future.get();

  ASSERT_EQ(0U, cq_impl_->size());

  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncBulkApplyTest, TooManyFailures) {
  bigtable::BulkMutation mut{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };

  // We give up on the 3rd error.
  auto constexpr kErrorCount = 2;

  EXPECT_CALL(*client_, PrepareAsyncMutateRows)
      .Times(kErrorCount + 1)
      .WillRepeatedly([](grpc::ClientContext*,
                         btproto::MutateRowsRequest const&,
                         grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>();
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
        });
        EXPECT_CALL(*reader, StartCall);
        return reader;
      });

  auto limited_retry_policy = LimitedErrorCountRetryPolicy(kErrorCount);
  auto bulk_apply_future = internal::AsyncRetryBulkApply::Create(
      cq_, limited_retry_policy.clone(), rpc_backoff_policy_->clone(),
      *idempotent_mutation_policy_, metadata_update_policy_, client_,
      "my-app-profile", "my-table", std::move(mut));

  for (int retry = 0; retry < kErrorCount; ++retry) {
    SimulateIteration();
    // simulate the backoff timer
    cq_impl_->SimulateCompletion(true);
    ASSERT_EQ(1U, cq_impl_->size());
  }

  SimulateIteration();

  auto failures = StatusOnly(bulk_apply_future.get());
  EXPECT_THAT(failures, ElementsAre(StatusIs(StatusCode::kUnavailable),
                                    StatusIs(StatusCode::kUnavailable)));

  ASSERT_EQ(0U, cq_impl_->size());
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncBulkApplyTest, UsesBackoffPolicy) {
  bigtable::BulkMutation mut{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };

  auto grpc_error = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
  auto error = MakeStatusFromRpcError(grpc_error);

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, Setup).Times(2);
  EXPECT_CALL(*mock, OnCompletion(error)).WillOnce([](Status const&) {
    return 10_ms;
  });

  EXPECT_CALL(*client_, PrepareAsyncMutateRows)
      .WillOnce([grpc_error](grpc::ClientContext*,
                             btproto::MutateRowsRequest const&,
                             grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>();
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish)
            .WillOnce([grpc_error](grpc::Status* status, void*) {
              *status = grpc_error;
            });
        EXPECT_CALL(*reader, StartCall);
        return reader;
      })
      .WillOnce([](grpc::ClientContext*, btproto::MutateRowsRequest const&,
                   grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>();
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
        EXPECT_CALL(*reader, StartCall);
        return reader;
      });

  auto bulk_apply_future = internal::AsyncRetryBulkApply::Create(
      cq_, rpc_retry_policy_->clone(), std::move(mock),
      *idempotent_mutation_policy_, metadata_update_policy_, client_,
      "my-app-profile", "my-table", std::move(mut));

  SimulateIteration();
  // simulate the backoff timer
  cq_impl_->SimulateCompletion(true);

  ASSERT_EQ(1U, cq_impl_->size());

  SimulateIteration();

  ASSERT_EQ(0U, cq_impl_->size());
}

TEST_F(AsyncBulkApplyTest, CancelDuringBackoff) {
  bigtable::BulkMutation mut{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };

  auto grpc_error = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
  auto error = MakeStatusFromRpcError(grpc_error);

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, Setup);
  EXPECT_CALL(*mock, OnCompletion(error)).WillOnce([](Status const&) {
    return 10_ms;
  });

  EXPECT_CALL(*client_, PrepareAsyncMutateRows)
      .WillOnce([grpc_error](grpc::ClientContext*,
                             btproto::MutateRowsRequest const&,
                             grpc::CompletionQueue*) {
        auto reader = absl::make_unique<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>();
        EXPECT_CALL(*reader, Read).Times(2);
        EXPECT_CALL(*reader, Finish)
            .WillOnce([grpc_error](grpc::Status* status, void*) {
              *status = grpc_error;
            });
        EXPECT_CALL(*reader, StartCall);
        return reader;
      });

  auto bulk_apply_future = internal::AsyncRetryBulkApply::Create(
      cq_, rpc_retry_policy_->clone(), std::move(mock),
      *idempotent_mutation_policy_, metadata_update_policy_, client_,
      "my-app-profile", "my-table", std::move(mut));

  SimulateIteration();
  ASSERT_EQ(1U, cq_impl_->size());

  // cancel the pending operation.
  bulk_apply_future.cancel();
  // simulate the backoff timer expiring.
  cq_impl_->SimulateCompletion(false);

  ASSERT_EQ(0U, cq_impl_->size());

  auto failures = StatusOnly(bulk_apply_future.get());
  EXPECT_THAT(failures, ElementsAre(StatusIs(StatusCode::kUnavailable),
                                    StatusIs(StatusCode::kUnavailable)));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
