// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/query_plan.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/synchronization/barrier.h"
#include "google/bigtable/v2/data.pb.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/time_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::FakeSystemClock;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;

TEST(QueryPlanTest, ResponseDataWithOriginalValidQueryPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(fake_cq_impl);

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("test-query");
  auto constexpr kResultMetadataText = R"pb(
    proto_schema {
      columns {
        name: "user_id"
        type { string_type {} }
      }
    }
  )pb";
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kResultMetadataText,
                                                            &metadata));
  *response.mutable_metadata() = metadata;

  auto plan = QueryPlan::Create(cq, response, [] {
    return make_ready_future(
        make_status_or(google::bigtable::v2::PrepareQueryResponse{}));
  });

  auto response_data = plan->response();
  ASSERT_STATUS_OK(response_data);
  EXPECT_EQ(response_data->prepared_query(), "test-query");
  EXPECT_THAT(response_data->metadata(), IsProtoEqual(metadata));

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, RefreshExpiredPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  google::bigtable::v2::PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  auto refresh_fn = [&]() {
    return make_ready_future(make_status_or(refresh_response));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  fake_cq_impl->SimulateCompletion(true);

  data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "refreshed-query-plan");

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, FailedRefreshExpiredPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  auto failed_refresh = internal::InternalError("oops!");
  auto refresh_fn = [&]() {
    return make_ready_future(
        StatusOr<google::bigtable::v2::PrepareQueryResponse>(failed_refresh));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  fake_cq_impl->SimulateCompletion(true);

  data = query_plan->response();
  EXPECT_THAT(data.status(), StatusIs(StatusCode::kInternal, "oops!"));

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, RefreshInvalidatedPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  google::bigtable::v2::PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  auto refresh_fn = [&]() {
    return make_ready_future(make_status_or(refresh_response));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  auto invalid_status = internal::InternalError("oops!");
  query_plan->Invalidate(invalid_status, data->prepared_query());

  data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "refreshed-query-plan");

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, FailedRefreshInvalidatedPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  auto failed_refresh = internal::InternalError("oops again!");
  auto refresh_fn = [&]() {
    return make_ready_future(
        StatusOr<google::bigtable::v2::PrepareQueryResponse>(failed_refresh));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  auto invalid_status = internal::InternalError("oops!");
  query_plan->Invalidate(invalid_status, data->prepared_query());

  data = query_plan->response();
  EXPECT_THAT(data.status(), StatusIs(StatusCode::kInternal, "oops again!"));

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, CreateFailedPlanAndRefresh) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  google::bigtable::v2::PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  auto refresh_fn = [&]() {
    return make_ready_future(make_status_or(refresh_response));
  };

  StatusOr<google::bigtable::v2::PrepareQueryResponse> response =
      Status(StatusCode::kResourceExhausted, "I'm tired boss");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  EXPECT_THAT(*fake_cq_impl, IsEmpty());

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "refreshed-query-plan");

  EXPECT_EQ(fake_cq_impl->size(), 1);
  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

// TODO(#15695): For reasons not yet understood, the fedora m32 CI build has
//   failures not seen in m64 builds when the number of threads is "too" high.
//  These failures occur while trying to create more than 500 threads.
constexpr int LimitNumThreadsOn32Bit(int num_threads) {
#if INTPTR_MAX == INT32_MAX
  return std::min(num_threads, 200);
#else
  return num_threads;
#endif
}

TEST(QueryPlanMultithreadedTest, RefreshInvalidatedPlan) {
  using google::bigtable::v2::PrepareQueryResponse;
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  int calls_to_refresh_fn = 0;
  std::mutex mu;
  auto refresh_fn = [&]() {
    std::lock_guard<std::mutex> lock(mu);
    ++calls_to_refresh_fn;
    return make_ready_future(make_status_or(refresh_response));
  };

  PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  constexpr int kNumThreads = LimitNumThreadsOn32Bit(1000);
  std::vector<std::thread> threads(kNumThreads);
  std::array<promise<StatusOr<PrepareQueryResponse>>, kNumThreads>
      data_responses;
  std::array<future<StatusOr<PrepareQueryResponse>>, kNumThreads>
      data_response_futures;
  for (auto i = 0; i < kNumThreads; ++i) {
    data_responses[i] = promise<StatusOr<PrepareQueryResponse>>();
    data_response_futures[i] = data_responses[i].get_future();
  }

  auto barrier = std::make_shared<absl::Barrier>(kNumThreads + 1);
  auto thread_fn = [barrier, query_plan](
                       promise<StatusOr<PrepareQueryResponse>>* thread_data) {
    barrier->Block();
    thread_data->set_value(query_plan->response());
  };

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(thread_fn, &(data_responses[i]));
  }

  auto invalid_status = internal::InternalError("oops!");
  query_plan->Invalidate(invalid_status, data->prepared_query());
  barrier->Block();

  for (auto& f : data_response_futures) {
    auto r = f.get();
    ASSERT_STATUS_OK(r);
    EXPECT_EQ(r->prepared_query(), "refreshed-query-plan");
  }

  for (auto& t : threads) {
    if (t.joinable()) t.join();
  }
  EXPECT_EQ(calls_to_refresh_fn, 1);

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanMultithreadedTest, RefreshInvalidatedPlanTransientFailures) {
  using google::bigtable::v2::PrepareQueryResponse;
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  int calls_to_refresh_fn = 0;
  std::mutex mu;
  auto refresh_fn = [&]() {
    std::lock_guard<std::mutex> lock(mu);
    ++calls_to_refresh_fn;
    if (calls_to_refresh_fn <= 3) {
      return make_ready_future(StatusOr<PrepareQueryResponse>(
          Status(StatusCode::kUnavailable, "hello?")));
    }
    return make_ready_future(make_status_or(refresh_response));
  };

  PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  constexpr int kNumThreads = LimitNumThreadsOn32Bit(1000);
  std::vector<std::thread> threads(kNumThreads);
  std::array<promise<StatusOr<PrepareQueryResponse>>, kNumThreads>
      data_responses;
  std::array<future<StatusOr<PrepareQueryResponse>>, kNumThreads>
      data_response_futures;
  for (auto i = 0; i < kNumThreads; ++i) {
    data_responses[i] = promise<StatusOr<PrepareQueryResponse>>();
    data_response_futures[i] = data_responses[i].get_future();
  }

  auto barrier = std::make_shared<absl::Barrier>(kNumThreads + 1);
  auto thread_fn = [barrier, query_plan](
                       promise<StatusOr<PrepareQueryResponse>>* thread_data) {
    barrier->Block();
    auto response = query_plan->response();
    while (!response.ok()) {
      std::this_thread::yield();
      response = query_plan->response();
    }
    thread_data->set_value(response);
  };

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(thread_fn, &(data_responses[i]));
  }

  auto invalid_status = internal::InternalError("oops!");
  query_plan->Invalidate(invalid_status, data->prepared_query());
  barrier->Block();

  for (auto& f : data_response_futures) {
    auto r = f.get();
    ASSERT_STATUS_OK(r);
    EXPECT_EQ(r->prepared_query(), "refreshed-query-plan");
  }

  for (auto& t : threads) {
    if (t.joinable()) t.join();
  }

  EXPECT_EQ(calls_to_refresh_fn, 4);

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanMultithreadedTest, RefreshInvalidatedPlanAfterFailedRefresh) {
  using google::bigtable::v2::PrepareQueryResponse;
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  constexpr int kNumFailingThreads = 100;
  PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  int calls_to_refresh_fn = 0;
  std::mutex mu;
  auto refresh_fn = [&]() {
    std::lock_guard<std::mutex> lock(mu);
    ++calls_to_refresh_fn;
    if (calls_to_refresh_fn <= kNumFailingThreads) {
      return make_ready_future(StatusOr<PrepareQueryResponse>(
          Status(StatusCode::kUnavailable, "hello?")));
    }
    return make_ready_future(make_status_or(refresh_response));
  };

  PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  std::vector<std::thread> failing_threads(kNumFailingThreads);
  std::array<StatusOr<PrepareQueryResponse>, kNumFailingThreads>
      failed_responses;

  auto fail_barrier = std::make_shared<absl::Barrier>(kNumFailingThreads + 1);
  auto fail_thread_fn =
      [fail_barrier, query_plan](StatusOr<PrepareQueryResponse>* thread_data) {
        fail_barrier->Block();
        *thread_data = query_plan->response();
      };

  for (int i = 0; i < kNumFailingThreads; ++i) {
    failed_responses[i] = StatusOr<PrepareQueryResponse>(
        Status(StatusCode::kNotFound, "not found"));
    failing_threads.emplace_back(fail_thread_fn, &(failed_responses[i]));
  }

  auto invalid_status = internal::InternalError("oops!");
  query_plan->Invalidate(invalid_status, data->prepared_query());
  fail_barrier->Block();

  for (auto& t : failing_threads) {
    if (t.joinable()) t.join();
  }

  EXPECT_EQ(calls_to_refresh_fn, kNumFailingThreads);
  for (auto const& r : failed_responses) {
    EXPECT_THAT(r.status(), StatusIs(StatusCode::kUnavailable, "hello?"));
  }

  // At this point, all the previous threads have run to completion and the
  // QueryPlan is left with a failed Status, not a valid response_. The state_
  // of QueryPlan is RefreshState::kBegin, so the first thread to call
  // response() should trigger a new refresh with the other threads waiting for
  // it to complete.
  constexpr int kNumThreads = LimitNumThreadsOn32Bit(1000);
  std::vector<std::thread> threads(kNumThreads);
  std::array<promise<StatusOr<PrepareQueryResponse>>, kNumThreads>
      data_responses;
  std::array<future<StatusOr<PrepareQueryResponse>>, kNumThreads>
      data_response_futures;
  for (auto i = 0; i < kNumThreads; ++i) {
    data_responses[i] = promise<StatusOr<PrepareQueryResponse>>();
    data_response_futures[i] = data_responses[i].get_future();
  }

  auto barrier = std::make_shared<absl::Barrier>(kNumThreads + 1);
  auto thread_fn = [barrier, query_plan](
                       promise<StatusOr<PrepareQueryResponse>>* thread_data) {
    barrier->Block();
    thread_data->set_value(query_plan->response());
  };

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(thread_fn, &(data_responses[i]));
  }

  barrier->Block();

  for (auto& f : data_response_futures) {
    auto r = f.get();
    ASSERT_STATUS_OK(r);
    EXPECT_EQ(r->prepared_query(), "refreshed-query-plan");
  }

  for (auto& t : threads) {
    if (t.joinable()) t.join();
  }
  EXPECT_EQ(calls_to_refresh_fn, kNumFailingThreads + 1);

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, ExpiredRefreshDoesNotDeadlockSingleThreadCompletionQueue) {
  internal::AutomaticallyCreatedBackgroundThreads background(1);
  CompletionQueue cq = background.cq();

  std::promise<void> refresh_completed;
  std::future<void> refresh_completed_future = refresh_completed.get_future();
  std::atomic<int> refresh_calls{0};

  google::bigtable::v2::PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  *refresh_response.mutable_valid_until() = internal::ToProtoTimestamp(
      std::chrono::system_clock::now() + std::chrono::hours(1));

  auto refresh_fn = [&cq, &refresh_calls, &refresh_completed,
                     refresh_response]() {
    int const call_num = ++refresh_calls;
    auto p = std::make_shared<
        promise<StatusOr<google::bigtable::v2::PrepareQueryResponse>>>();
    future<StatusOr<google::bigtable::v2::PrepareQueryResponse>> f =
        p->get_future();
    // Resolve the refresh RPC on the exact same 1-thread CompletionQueue that
    // runs the deadline timer callback. Blocking inline on future::get() inside
    // the timer callback would permanently self-deadlock this CQ thread.
    cq.RunAsync([p, refresh_response, call_num, &refresh_completed]() mutable {
      p->set_value(std::move(refresh_response));
      if (call_num == 1) refresh_completed.set_value();
    });
    return f;
  };

  google::bigtable::v2::PrepareQueryResponse initial_response;
  initial_response.set_prepared_query("original-query-plan");
  *initial_response.mutable_valid_until() = internal::ToProtoTimestamp(
      std::chrono::system_clock::now() + std::chrono::milliseconds(500));

  {
    std::shared_ptr<QueryPlan> query_plan =
        QueryPlan::Create(cq, initial_response, refresh_fn);
    ASSERT_EQ(refresh_completed_future.wait_for(std::chrono::seconds(5)),
              std::future_status::ready);
    EXPECT_EQ(refresh_calls.load(), 1);

    StatusOr<google::bigtable::v2::PrepareQueryResponse> data =
        query_plan->response();
    ASSERT_STATUS_OK(data);
    EXPECT_EQ(data->prepared_query(), "refreshed-query-plan");
  }

  background.Shutdown();
}

TEST(QueryPlanTest, StaleInvalidationIgnoredAfterMultipleRefreshes) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  int refresh_count = 0;
  auto refresh_fn = [&refresh_count]() {
    ++refresh_count;
    google::bigtable::v2::PrepareQueryResponse refreshed;
    refreshed.set_prepared_query("plan-" + std::to_string(refresh_count));
    return make_ready_future(make_status_or(std::move(refreshed)));
  };

  google::bigtable::v2::PrepareQueryResponse initial;
  initial.set_prepared_query("plan-0");
  std::shared_ptr<QueryPlan> query_plan =
      QueryPlan::Create(CompletionQueue(fake_cq_impl), initial, refresh_fn);

  // Refresh #1: plan-0 -> plan-1
  fake_cq_impl->SimulateCompletion(true);
  StatusOr<google::bigtable::v2::PrepareQueryResponse> data =
      query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "plan-1");

  // Refresh #2: plan-1 -> plan-2
  fake_cq_impl->SimulateCompletion(true);
  data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "plan-2");
  EXPECT_EQ(refresh_count, 2);

  // Late invalidation referencing plan-0 must not invalidate plan-2.
  query_plan->Invalidate(internal::FailedPreconditionError("expired"),
                         "plan-0");
  data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "plan-2");
  EXPECT_EQ(refresh_count, 2);

  // Invalidation matching active plan-2 triggers Refresh #3 -> plan-3.
  query_plan->Invalidate(internal::FailedPreconditionError("expired"),
                         "plan-2");
  data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "plan-3");
  EXPECT_EQ(refresh_count, 3);

  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, InvalidateDuringPendingProactiveRefreshWaitsForRefresh) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  promise<StatusOr<google::bigtable::v2::PrepareQueryResponse>> refresh_promise;
  std::atomic<int> refresh_calls{0};
  auto refresh_fn = [&]() {
    ++refresh_calls;
    return refresh_promise.get_future();
  };

  google::bigtable::v2::PrepareQueryResponse initial;
  initial.set_prepared_query("plan-0");
  std::shared_ptr<QueryPlan> query_plan =
      QueryPlan::Create(CompletionQueue(fake_cq_impl), initial, refresh_fn);

  // Fire timer so proactive refresh starts (state == kPending), while refresh
  // RPC is still in flight.
  fake_cq_impl->SimulateCompletion(true);
  EXPECT_EQ(refresh_calls.load(), 1);

  // Invalidate plan-0 while the proactive refresh is in flight.
  query_plan->Invalidate(internal::FailedPreconditionError("expired"),
                         "plan-0");

  std::promise<StatusOr<google::bigtable::v2::PrepareQueryResponse>>
      caller_result;
  std::future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>
      caller_future = caller_result.get_future();
  std::thread caller([query_plan, &caller_result] {
    caller_result.set_value(query_plan->response());
  });

  // Caller must wait for the in-flight refresh rather than returning plan-0 or
  // starting a duplicate refresh RPC.
  EXPECT_EQ(caller_future.wait_for(std::chrono::milliseconds(20)),
            std::future_status::timeout);
  EXPECT_EQ(refresh_calls.load(), 1);

  google::bigtable::v2::PrepareQueryResponse refreshed;
  refreshed.set_prepared_query("plan-1");
  refresh_promise.set_value(refreshed);

  ASSERT_EQ(caller_future.wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  caller.join();

  StatusOr<google::bigtable::v2::PrepareQueryResponse> data =
      caller_future.get();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "plan-1");
  EXPECT_EQ(refresh_calls.load(), 1);

  fake_cq_impl->SimulateCompletion(false);
}

#include "google/cloud/internal/disable_deprecation_warnings.inc"
TEST(QueryPlanTest, DeprecatedAccessors) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("deprecated-accessor-plan");
  char constexpr kResultMetadataText[] = R"pb(
    proto_schema {
      columns {
        name: "col1"
        type { string_type {} }
      }
    }
  )pb";
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kResultMetadataText,
                                                            &metadata));
  *response.mutable_metadata() = metadata;

  std::shared_ptr<QueryPlan> plan =
      QueryPlan::Create(CompletionQueue(fake_cq_impl), response, [] {
        return make_ready_future<
            StatusOr<google::bigtable::v2::PrepareQueryResponse>>(
            internal::InternalError("refresh failed"));
      });

  StatusOr<std::string> pq = plan->prepared_query();
  ASSERT_STATUS_OK(pq);
  EXPECT_EQ(*pq, "deprecated-accessor-plan");

  StatusOr<google::bigtable::v2::ResultSetMetadata> md = plan->metadata();
  ASSERT_STATUS_OK(md);
  EXPECT_THAT(*md, IsProtoEqual(metadata));

  plan->Invalidate(internal::FailedPreconditionError("expired"),
                   "deprecated-accessor-plan");
  EXPECT_THAT(plan->prepared_query(),
              StatusIs(StatusCode::kInternal, "refresh failed"));
  EXPECT_THAT(plan->metadata(),
              StatusIs(StatusCode::kInternal, "refresh failed"));

  fake_cq_impl->SimulateCompletion(false);
}
#include "google/cloud/internal/diagnostics_pop.inc"

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
