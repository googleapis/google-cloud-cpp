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

#include "google/cloud/bigtable/internal/async_bulk_apply.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <grpcpp/support/status_code_enum.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ms = std::chrono::milliseconds;
using ::google::cloud::bigtable::DataLimitedErrorCountRetryPolicy;
using ::google::cloud::bigtable::testing::MockAsyncMutateRowsStream;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::ElementsAreArray;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Property;

auto constexpr kNumRetries = 2;
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kAppProfile = "the-profile";

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try again");
}

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "fail");
}

bigtable::SingleRowMutation IdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(
      row_key, {bigtable::SetCell("fam", "col", ms(0), "val")});
}

bigtable::SingleRowMutation NonIdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(row_key,
                                     {bigtable::SetCell("fam", "col", "val")});
}

Matcher<v2::MutateRowsRequest::Entry> MatchEntry(std::string const& row_key) {
  return Property(&v2::MutateRowsRequest::Entry::row_key, row_key);
}

// Individual entry pairs are: {index, StatusCode}
absl::optional<v2::MutateRowsResponse> MakeResponse(
    std::vector<std::pair<int, grpc::StatusCode>> const& entries) {
  v2::MutateRowsResponse resp;
  for (auto entry : entries) {
    auto& e = *resp.add_entries();
    e.set_index(entry.first);
    e.mutable_status()->set_code(entry.second);
  }
  return resp;
}

void CheckFailedMutations(
    std::vector<bigtable::FailedMutation> const& actual,
    std::vector<bigtable::FailedMutation> const& expected) {
  struct Unroll {
    explicit Unroll(std::vector<bigtable::FailedMutation> const& failed) {
      for (auto const& f : failed) {
        statuses.push_back(f.status().code());
        indices.push_back(f.original_index());
      }
    }
    std::vector<StatusCode> statuses;
    std::vector<int> indices;
  };

  auto a = Unroll(actual);
  auto e = Unroll(expected);
  EXPECT_THAT(a.statuses, ElementsAreArray(e.statuses));
  EXPECT_THAT(a.indices, ElementsAreArray(e.indices));
}

TEST(AsyncBulkApplyTest, NoMutations) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, bigtable::BulkMutation());

  CheckFailedMutations(actual.get(), {});
}

TEST(AsyncBulkApplyTest, Success) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(MatchEntry("r0"), MatchEntry("r1")));
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{1, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, std::move(mut));

  CheckFailedMutations(actual.get(), {});
}

TEST(AsyncBulkApplyTest, PartialStreamIsRetried) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(MatchEntry("r0"), MatchEntry("r1")));
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        // This first stream only returns one of the two entries.
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      })
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(MatchEntry("r1")));
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, std::move(mut));

  CheckFailedMutations(actual.get(), {});
}

TEST(AsyncBulkApplyTest, IdempotentMutationPolicy) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 2},
                                                    {TransientError(), 3}};
  bigtable::BulkMutation mut(
      IdempotentMutation("success"),
      IdempotentMutation("retry-transient-error"),
      IdempotentMutation("fail-with-permanent-error"),
      NonIdempotentMutation("fail-with-transient-error"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK},
                                {1, grpc::StatusCode::UNAVAILABLE},
                                {2, grpc::StatusCode::PERMISSION_DENIED},
                                {3, grpc::StatusCode::UNAVAILABLE}}));
            })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      })
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(MatchEntry("retry-transient-error")));
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, std::move(mut));

  CheckFailedMutations(actual.get(), expected);
}

TEST(AsyncBulkApplyTest, TooManyStreamFailures) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](CompletionQueue const&,
                         std::unique_ptr<grpc::ClientContext>,
                         v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(MatchEntry("r0")));
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(kNumRetries)
      .WillRepeatedly([] {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(kNumRetries + 1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, std::move(mut));

  CheckFailedMutations(actual.get(), expected);
}

TEST(AsyncBulkApplyTest, TimerError) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kUnavailable, "try again"));
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future<StatusOr<std::chrono::system_clock::time_point>>(
        Status(StatusCode::kDeadlineExceeded, "timer error"));
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, std::move(mut));

  CheckFailedMutations(actual.get(), expected);
}

TEST(AsyncBulkApplyTest, CancelAfterSuccess) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"));
  promise<absl::optional<v2::MutateRowsResponse>> p;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([&p](CompletionQueue const&,
                     std::unique_ptr<grpc::ClientContext>,
                     v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            // We block here so the caller can cancel the request. The value
            // returned will be empty, meaning the stream is complete.
            .WillOnce([&p] { return p.get_future(); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, std::move(mut));

  // Cancel the call after performing the one and only read of this test stream.
  actual.cancel();
  // Proceed with the rest of the stream. In this test, there are no more
  // responses to be read. The client call should succeed.
  p.set_value(absl::optional<v2::MutateRowsResponse>{});
  CheckFailedMutations(actual.get(), {});
}

TEST(AsyncBulkApplyTest, CancelMidStream) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kCancelled, "User cancelled"), 2}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"),
                             IdempotentMutation("r2"));
  promise<absl::optional<v2::MutateRowsResponse>> p;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([&p](CompletionQueue const&,
                     std::unique_ptr<grpc::ClientContext>,
                     v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            // We block here so the caller can cancel the request. The value
            // returned will be a response, meaning the stream is still active
            // and needs to be drained.
            .WillOnce([&p] { return p.get_future(); });
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(absl::optional<v2::MutateRowsResponse>{});
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kCancelled, "User cancelled"));
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::move(retry), std::move(mock_b), *idempotency, kAppProfile,
      kTableName, std::move(mut));

  // Cancel the call after performing one read of this test stream.
  actual.cancel();
  // Proceed with the rest of the stream. In this test, there are more responses
  // to be read, which we must drain. The client call should fail.
  p.set_value(MakeResponse({{1, grpc::StatusCode::OK}}));
  CheckFailedMutations(actual.get(), expected);
}

TEST(AsyncBulkApplyTest, CurrentOptionsContinuedOnRetries) {
  struct TestOption {
    using Type = int;
  };

  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kUnavailable, "try again"), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(2)
      .WillRepeatedly([](CompletionQueue const&,
                         std::unique_ptr<grpc::ClientContext>,
                         v2::MutateRowsRequest const&) {
        EXPECT_EQ(5, internal::CurrentOptions().get<TestOption>());
        auto stream = absl::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kUnavailable, "try again"));
        });
        return stream;
      });

  promise<StatusOr<std::chrono::system_clock::time_point>> timer_promise;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([&timer_promise] {
    return timer_promise.get_future();
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(1).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);

  internal::OptionsSpan span(
      Options{}
          .set<internal::GrpcSetupOption>(mock_setup.AsStdFunction())
          .set<TestOption>(5));
  auto fut = AsyncBulkApplier::Create(cq, mock, std::move(retry),
                                      std::move(mock_b), *idempotency,
                                      kAppProfile, kTableName, std::move(mut));

  // Simulate the timer being satisfied in a thread with different prevailing
  // options than the calling thread.
  internal::OptionsSpan clear(Options{});
  timer_promise.set_value(make_status_or(std::chrono::system_clock::now()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
