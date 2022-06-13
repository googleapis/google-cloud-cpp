// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/internal/async_row_sampler.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ms = std::chrono::milliseconds;
using ::google::cloud::bigtable::testing::MockAsyncSampleRowKeysStream;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::MockFunction;

auto constexpr kNumRetries = 2;
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kAppProfile = "the-profile";

struct RowKeySampleVectors {
  explicit RowKeySampleVectors(std::vector<bigtable::RowKeySample> samples) {
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

absl::optional<v2::SampleRowKeysResponse> MakeResponse(std::string row_key,
                                                       std::int64_t offset) {
  v2::SampleRowKeysResponse r;
  r.set_row_key(std::move(row_key));
  r.set_offset_bytes(offset);
  return absl::make_optional(r);
};

TEST(AsyncSampleRowKeysTest, Simple) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("test1", 11)); })
            .WillOnce(
                [] { return make_ready_future(MakeResponse("test2", 22)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), kAppProfile, kTableName)
                 .get();

  ASSERT_STATUS_OK(sor);
  auto samples = RowKeySampleVectors(*sor);
  EXPECT_THAT(samples.row_keys, ElementsAre("test1", "test2"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(11, 22));
}

TEST(AsyncSampleRowKeysTest, RetryResetsSamples) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("forgotten", 11)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kUnavailable, "try again"));
        });
        return stream;
      })
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("returned", 22)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), kAppProfile, kTableName)
                 .get();

  ASSERT_STATUS_OK(sor);
  auto samples = RowKeySampleVectors(*sor);
  EXPECT_THAT(samples.row_keys, ElementsAre("returned"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(22));
}

TEST(AsyncSampleRowKeysTest, TooManyFailures) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](CompletionQueue const&,
                         std::unique_ptr<grpc::ClientContext>,
                         v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncSampleRowKeysStream>();
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(kNumRetries + 1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), kAppProfile, kTableName)
                 .get();

  EXPECT_THAT(sor, StatusIs(StatusCode::kUnavailable, HasSubstr("try again")));
}

TEST(AsyncSampleRowKeysTest, TimerError) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncSampleRowKeysStream>();
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), kAppProfile, kTableName)
                 .get();
  // If the TimerFuture returns a bad status, it is almost always because the
  // call has been cancelled. So it is more informative for the sampler to
  // return "call cancelled" than to pass along the exact error.
  EXPECT_THAT(sor,
              StatusIs(StatusCode::kCancelled, HasSubstr("call cancelled")));
}

TEST(AsyncSampleRowKeysTest, CancelAfterSuccess) {
  promise<absl::optional<v2::SampleRowKeysResponse>> p;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([&p](CompletionQueue const&,
                     std::unique_ptr<grpc::ClientContext>,
                     v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("test1", 11)); })
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto fut = AsyncRowSampler::Create(
      cq, mock, std::move(retry), std::move(mock_b), kAppProfile, kTableName);
  // Cancel the call after performing the one and only read of this test stream.
  fut.cancel();
  // Proceed with the rest of the stream. In this test, there are no more
  // responses to be read. The client call should succeed.
  p.set_value(absl::optional<v2::SampleRowKeysResponse>{});
  auto sor = fut.get();
  ASSERT_STATUS_OK(sor);
  auto samples = RowKeySampleVectors(*sor);
  EXPECT_THAT(samples.row_keys, ElementsAre("test1"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(11));
}

TEST(AsyncSampleRowKeysTest, CancelMidStream) {
  promise<absl::optional<v2::SampleRowKeysResponse>> p;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([&p](CompletionQueue const&,
                     std::unique_ptr<grpc::ClientContext>,
                     v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncSampleRowKeysStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(MakeResponse("forgotten1", 11));
            })
            // We block here so the caller can cancel the request. The value
            // returned will be a response, meaning the stream is still active
            // and needs to be drained.
            .WillOnce([&p] { return p.get_future(); });
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("discarded", 33)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto fut = AsyncRowSampler::Create(
      cq, mock, std::move(retry), std::move(mock_b), kAppProfile, kTableName);
  // Cancel the call after performing one read of this test stream.
  fut.cancel();
  // Proceed with the rest of the stream. In this test, there are more responses
  // to be read, which we must drain. The client call should fail.
  p.set_value(MakeResponse("forgotten2", 22));
  auto sor = fut.get();
  EXPECT_THAT(sor,
              StatusIs(StatusCode::kCancelled, HasSubstr("User cancelled")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
