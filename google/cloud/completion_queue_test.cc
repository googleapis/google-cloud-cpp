// Copyright 2019 Google LLC
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

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/default_completion_queue_impl.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <gmock/gmock.h>
#include <chrono>
#include <memory>
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

namespace btadmin = ::google::bigtable::admin::v2;
namespace btproto = ::google::bigtable::v2;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::StrictMock;

class MockClient {
 public:
  MOCK_METHOD3(
      AsyncGetTable,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(
          grpc::ClientContext*, btadmin::GetTableRequest const&,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(AsyncReadRows,
               std::unique_ptr<::grpc::ClientAsyncReaderInterface<
                   btproto::ReadRowsResponse>>(grpc::ClientContext*,
                                               btproto::ReadRowsRequest const&,
                                               grpc::CompletionQueue* cq));
};

class MockTableReader
    : public grpc::ClientAsyncResponseReaderInterface<btadmin::Table> {
 public:
  MOCK_METHOD0(StartCall, void());
  MOCK_METHOD1(ReadInitialMetadata, void(void*));
  MOCK_METHOD3(Finish, void(btadmin::Table*, grpc::Status*, void*));
};

class MockRowReader
    : public grpc::ClientAsyncReaderInterface<btproto::ReadRowsResponse> {
 public:
  MOCK_METHOD1(StartCall, void(void*));
  MOCK_METHOD1(ReadInitialMetadata, void(void*));
  MOCK_METHOD2(Read, void(btproto::ReadRowsResponse*, void*));
  MOCK_METHOD2(Finish, void(grpc::Status*, void*));
};

class MockOperation : public internal::AsyncGrpcOperation {
 public:
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD1(Notify, bool(bool));
};

/// @test A regression test for #5141
TEST(CompletionQueueTest, TimerCancel) {
  // There are a lot of magical numbers in this test, there were tuned to #5141
  // 99 out of 100 times on my workstation.
  CompletionQueue cq;
  std::vector<std::thread> runners;
  std::generate_n(std::back_inserter(runners), 4, [&cq] {
    return std::thread([](CompletionQueue c) { c.Run(); }, cq);
  });

  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;
  auto worker = [&](CompletionQueue cq) {
    for (int i = 0; i != 10000; ++i) {
      std::vector<TimerFuture> timers;
      for (int j = 0; j != 10; ++j) {
        timers.push_back(cq.MakeRelativeTimer(std::chrono::microseconds(10)));
      }
      cq.CancelAll();
      for (auto& t : timers) t.cancel();
    }
  };
  std::vector<std::thread> workers;
  std::generate_n(std::back_inserter(runners), 8,
                  [&] { return std::thread(worker, cq); });

  for (auto& t : workers) t.join();
  cq.Shutdown();
  for (auto& t : runners) t.join();
}

/// @test Verify that the basic functionality in a CompletionQueue works.
TEST(CompletionQueueTest, TimerSmokeTest) {
  CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  using ms = std::chrono::milliseconds;
  promise<void> wait_for_sleep;
  cq.MakeRelativeTimer(ms(2))
      .then([&wait_for_sleep](
                future<StatusOr<std::chrono::system_clock::time_point>>) {
        wait_for_sleep.set_value();
      })
      .get();

  auto f = wait_for_sleep.get_future();
  EXPECT_EQ(std::future_status::ready, f.wait_for(ms(0)));
  cq.Shutdown();
  t.join();
}

TEST(CompletionQueueTest, MockSmokeTest) {
  auto mock = std::make_shared<FakeCompletionQueueImpl>();

  CompletionQueue cq(mock);
  using ms = std::chrono::milliseconds;
  promise<void> wait_for_sleep;
  cq.MakeRelativeTimer(ms(20000)).then(
      [&wait_for_sleep](
          future<StatusOr<std::chrono::system_clock::time_point>>) {
        wait_for_sleep.set_value();
      });
  mock->SimulateCompletion(/*ok=*/true);

  auto f = wait_for_sleep.get_future();
  EXPECT_EQ(std::future_status::ready, f.wait_for(ms(0)));
  cq.Shutdown();
}

TEST(CompletionQueueTest, ShutdownWithPending) {
  using ms = std::chrono::milliseconds;

  future<void> timer;
  {
    CompletionQueue cq;
    std::thread runner([&cq] { cq.Run(); });
    // In very rare conditions the timer would expire before we get a chance to
    // call `cq.Shutdown()`, use a little loop to avoid this. In most runs this
    // loop will exit after the first iteration. By my measurements the loop
    // would need to run more than once every 10,000 runs. On the other hand,
    // this should prevent the flake reported in #4384.
    for (auto i = 0; i != 5; ++i) {
      timer = cq.MakeRelativeTimer(ms(500)).then(
          [](future<StatusOr<std::chrono::system_clock::time_point>> f) {
            // Timer still runs to completion after `Shutdown`.
            EXPECT_STATUS_OK(f.get().status());
          });
      if (timer.wait_for(ms(0)) == std::future_status::timeout) break;
      // This should be so rare that it is Okay to fail the test. We expect a
      // the following assertion to fail once every 10^25 runs, if we see even
      // one that means something is terribly wrong with my math and/or the
      // code.
      ASSERT_NE(i, 4);
    }
    cq.Shutdown();
    runner.join();
  }
  EXPECT_EQ(std::future_status::ready, timer.wait_for(ms(0)));
}

TEST(CompletionQueueTest, ShutdownWithPendingFake) {
  using ms = std::chrono::milliseconds;
  CompletionQueue cq(std::make_shared<FakeCompletionQueueImpl>());
  auto timer = cq.MakeRelativeTimer(ms(500));
  cq.CancelAll();
  cq.Shutdown();
  EXPECT_EQ(StatusCode::kCancelled, timer.get().status().code());
}

TEST(CompletionQueueTest, CanCancelAllEvents) {
  CompletionQueue cq;
  promise<void> done;
  std::thread runner([&cq, &done] {
    cq.Run();
    done.set_value();
  });
  for (int i = 0; i < 3; ++i) {
    using hours = std::chrono::hours;
    cq.MakeRelativeTimer(hours(1)).then(
        [](future<StatusOr<std::chrono::system_clock::time_point>> result) {
          // Cancelled timers return CANCELLED status.
          EXPECT_EQ(StatusCode::kCancelled, result.get().status().code());
        });
  }
  using ms = std::chrono::milliseconds;
  auto f = done.get_future();
  EXPECT_EQ(std::future_status::timeout, f.wait_for(ms(1)));
  cq.Shutdown();
  EXPECT_EQ(std::future_status::timeout, f.wait_for(ms(1)));
  cq.CancelAll();
  f.wait();
  EXPECT_TRUE(f.is_ready());
  runner.join();
}

TEST(CompletionQueueTest, MakeUnaryRpc) {
  using ms = std::chrono::milliseconds;

  auto mock_cq = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto mock_reader = absl::make_unique<MockTableReader>();
  EXPECT_CALL(*mock_reader, Finish(_, _, _))
      .WillOnce([](btadmin::Table* table, grpc::Status* status, void*) {
        table->set_name("test-table-name");
        *status = grpc::Status::OK;
      });
  MockClient mock_client;
  EXPECT_CALL(mock_client, AsyncGetTable(_, _, _))
      .WillOnce([&mock_reader](grpc::ClientContext*,
                               btadmin::GetTableRequest const& request,
                               grpc::CompletionQueue*) {
        EXPECT_EQ("test-table-name", request.name());
        // This looks like a double delete, but it is not because
        // std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<T>> is
        // specialized to not delete. :shrug:
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(
            mock_reader.get());
      });

  std::thread runner([&cq] { cq.Run(); });

  btadmin::GetTableRequest request;
  request.set_name("test-table-name");
  future<void> done =
      cq.MakeUnaryRpc(
            [&mock_client](grpc::ClientContext* context,
                           btadmin::GetTableRequest const& request,
                           grpc::CompletionQueue* cq) {
              return mock_client.AsyncGetTable(context, request, cq);
            },
            request, absl::make_unique<grpc::ClientContext>())
          .then([](future<StatusOr<btadmin::Table>> f) {
            auto table = f.get();
            ASSERT_STATUS_OK(table);
            EXPECT_EQ("test-table-name", table->name());
          });

  mock_cq->SimulateCompletion(true);

  EXPECT_EQ(std::future_status::ready, done.wait_for(ms(0)));

  cq.Shutdown();
  runner.join();
}

TEST(CompletionQueueTest, MakeStreamingReadRpc) {
  auto mock_cq = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto mock_reader = absl::make_unique<MockRowReader>();
  EXPECT_CALL(*mock_reader, StartCall(_)).Times(1);
  EXPECT_CALL(*mock_reader, Read(_, _)).Times(2);
  EXPECT_CALL(*mock_reader, Finish(_, _)).Times(1);

  MockClient mock_client;
  EXPECT_CALL(mock_client, AsyncReadRows(_, _, _))
      .WillOnce([&mock_reader](grpc::ClientContext*,
                               btproto::ReadRowsRequest const& request,
                               grpc::CompletionQueue*) {
        EXPECT_EQ("test-table-name", request.table_name());
        return std::unique_ptr<
            grpc::ClientAsyncReaderInterface<btproto::ReadRowsResponse>>(
            mock_reader.release());
      });

  std::thread runner([&cq] { cq.Run(); });

  btproto::ReadRowsRequest request;
  request.set_table_name("test-table-name");

  int on_read_counter = 0;
  int on_finish_counter = 0;
  (void)cq.MakeStreamingReadRpc(
      [&mock_client](grpc::ClientContext* context,
                     btproto::ReadRowsRequest const& request,
                     grpc::CompletionQueue* cq) {
        return mock_client.AsyncReadRows(context, request, cq);
      },
      request, absl::make_unique<grpc::ClientContext>(),
      [&on_read_counter](btproto::ReadRowsResponse const&) {
        ++on_read_counter;
        return make_ready_future(true);
      },
      [&on_finish_counter](Status const&) { ++on_finish_counter; });

  // Simulate the OnStart() completion
  mock_cq->SimulateCompletion(true);
  // Simulate the first Read() completion
  mock_cq->SimulateCompletion(true);
  EXPECT_EQ(1, on_read_counter);
  EXPECT_EQ(0, on_finish_counter);

  // Simulate a Read() returning false
  mock_cq->SimulateCompletion(false);
  EXPECT_EQ(1, on_read_counter);
  EXPECT_EQ(0, on_finish_counter);

  // Simulate the Finish() call completing asynchronously
  mock_cq->SimulateCompletion(false);
  EXPECT_EQ(1, on_read_counter);
  EXPECT_EQ(1, on_finish_counter);

  cq.Shutdown();
  runner.join();
}

TEST(CompletionQueueTest, MakeRpcsAfterShutdown) {
  using ms = std::chrono::milliseconds;

  auto mock_cq = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  // Use `StrictMock` to enforce that there are no calls made on the client.
  StrictMock<MockClient> mock_client;
  std::thread runner([&cq] { cq.Run(); });
  cq.Shutdown();

  btadmin::GetTableRequest get_table_request;
  get_table_request.set_name("test-table-name");
  future<void> done =
      cq.MakeUnaryRpc(
            [&mock_client](grpc::ClientContext* context,
                           btadmin::GetTableRequest const& request,
                           grpc::CompletionQueue* cq) {
              return mock_client.AsyncGetTable(context, request, cq);
            },
            get_table_request, absl::make_unique<grpc::ClientContext>())
          .then([](future<StatusOr<btadmin::Table>> f) {
            EXPECT_EQ(StatusCode::kCancelled, f.get().status().code());
          });

  btproto::ReadRowsRequest read_request;
  read_request.set_table_name("test-table-name");
  (void)cq.MakeStreamingReadRpc(
      [&mock_client](grpc::ClientContext* context,
                     btproto::ReadRowsRequest const& request,
                     grpc::CompletionQueue* cq) {
        return mock_client.AsyncReadRows(context, request, cq);
      },
      read_request, absl::make_unique<grpc::ClientContext>(),
      [](btproto::ReadRowsResponse const&) {
        ADD_FAILURE() << "OnReadHandler unexpectedly called";
        return make_ready_future(true);
      },
      [](Status const& status) {
        EXPECT_EQ(StatusCode::kCancelled, status.code());
      });

  mock_cq->SimulateCompletion(true);
  EXPECT_EQ(std::future_status::ready, done.wait_for(ms(0)));
  runner.join();
}

TEST(CompletionQueueTest, RunAsync) {
  CompletionQueue cq;

  std::thread runner([&cq] { cq.Run(); });

  std::promise<void> done_promise;
  cq.RunAsync([&done_promise](CompletionQueue&) { done_promise.set_value(); });

  auto done = done_promise.get_future();
  done.get();

  cq.Shutdown();
  runner.join();
}

TEST(CompletionQueueTest, RunAsyncVoid) {
  CompletionQueue cq;

  std::thread runner([&cq] { cq.Run(); });

  std::promise<void> done_promise;
  cq.RunAsync([&done_promise] { done_promise.set_value(); });

  auto done = done_promise.get_future();
  done.get();

  cq.Shutdown();
  runner.join();
}

TEST(CompletionQueueTest, RunAsyncCompletionQueueDestroyed) {
  auto cq_impl = std::make_shared<FakeCompletionQueueImpl>();

  std::promise<void> done_promise;
  {
    CompletionQueue cq(cq_impl);
    cq.RunAsync([&done_promise](CompletionQueue& cq) {
      done_promise.set_value();
      cq.Shutdown();
    });
  }
  cq_impl->SimulateCompletion(true);

  done_promise.get_future().get();
}

TEST(CompletionQueueTest, RunAsyncMoveOnly) {
  struct MoveOnly {
    promise<void> p;
    void operator()(CompletionQueue&) { p.set_value(); }
  };
  static_assert(!std::is_copy_assignable<MoveOnly>::value,
                "MoveOnly test type should not copy-assignable");

  promise<void> p;
  auto done = p.get_future();
  CompletionQueue cq;
  std::thread t{[&cq] { cq.Run(); }};
  cq.RunAsync(MoveOnly{std::move(p)});
  done.get();
  cq.Shutdown();
  t.join();
}

TEST(CompletionQueueTest, RunAsyncMoveOnlyVoid) {
  struct MoveOnly {
    promise<void> p;
    void operator()() { p.set_value(); }
  };
  static_assert(!std::is_copy_assignable<MoveOnly>::value,
                "MoveOnly test type should not copy-assignable");

  promise<void> p;
  auto done = p.get_future();
  CompletionQueue cq;
  std::thread t{[&cq] { cq.Run(); }};
  cq.RunAsync(MoveOnly{std::move(p)});
  done.get();
  cq.Shutdown();
  t.join();
}

TEST(CompletionQueueTest, RunAsyncThread) {
  CompletionQueue cq;

  std::set<std::thread::id> runner_ids;
  auto constexpr kRunners = 8;
  std::vector<std::thread> runners(kRunners);
  for (auto& t : runners) {
    promise<std::thread::id> started;
    auto f = started.get_future();
    t = std::thread(
        [&cq](promise<std::thread::id> p) {
          p.set_value(std::this_thread::get_id());
          cq.Run();
        },
        std::move(started));
    runner_ids.insert(f.get());
  }

  auto constexpr kIterations = 10000;
  std::vector<promise<std::thread::id>> pending(kIterations);
  std::vector<future<std::thread::id>> actual;
  for (int i = 0; i != kIterations; ++i) {
    auto& p = pending[i];
    actual.push_back(p.get_future());
    cq.RunAsync(
        [&p](CompletionQueue&) { p.set_value(std::this_thread::get_id()); });
  }

  for (auto& done : actual) {
    auto id = done.get();
    EXPECT_THAT(runner_ids, Contains(id));
  }

  cq.Shutdown();
  for (auto& t : runners) t.join();
}

TEST(CompletionQueueTest, NoRunAsyncAfterShutdown) {
  CompletionQueue cq;

  std::thread runner([&cq] { cq.Run(); });

  promise<void> a;
  promise<void> b;
  promise<void> c;
  cq.RunAsync([&] {
    a.set_value();
    b.get_future().wait();
  });
  a.get_future().wait();
  cq.Shutdown();
  b.set_value();
  cq.RunAsync([&c] { c.set_value(); });
  auto f = c.get_future();
  using ms = std::chrono::milliseconds;
  // After runner is joined there will be no activity in the CQ, so this is not
  // likely to flake. If the promise was ever going to be satisfied it would
  // have been
  runner.join();
  EXPECT_EQ(std::future_status::timeout, f.wait_for(ms(0)));
}

// Sets up a timer that reschedules itself and verifies we can shut down
// cleanly whether we call `CancelAll()` on the queue first or not.
namespace {
using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;

void RunAndReschedule(CompletionQueue& cq, bool ok,
                      std::chrono::seconds duration) {
  if (ok) {
    cq.MakeRelativeTimer(duration).then([&cq, duration](TimerFuture result) {
      RunAndReschedule(cq, result.get().ok(), duration);
    });
  }
}
}  // namespace

TEST(CompletionQueueTest, ShutdownWithReschedulingTimer) {
  CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  RunAndReschedule(cq, /*ok=*/true, std::chrono::seconds(1));

  cq.Shutdown();
  t.join();
  // This is a "neither crashed nor hung" test. Reaching this point is success.
  SUCCEED();
}

TEST(CompletionQueueTest, ShutdownWithFastReschedulingTimer) {
  auto constexpr kThreadCount = 32;
  auto constexpr kTimerCount = 100;
  CompletionQueue cq;
  std::vector<std::thread> threads(kThreadCount);
  std::generate_n(threads.begin(), threads.size(),
                  [&cq] { return std::thread([&cq] { cq.Run(); }); });

  for (int i = 0; i != kTimerCount; ++i) {
    RunAndReschedule(cq, /*ok=*/true, std::chrono::seconds(0));
  }

  promise<void> wait;
  cq.MakeRelativeTimer(std::chrono::milliseconds(1)).then([&wait](TimerFuture) {
    wait.set_value();
  });
  wait.get_future().get();
  cq.Shutdown();
  for (auto& t : threads) {
    t.join();
  }
  // This is a "neither crashed nor hung" test. Reaching this point is success.
  SUCCEED();
}

TEST(CompletionQueueTest, CancelAndShutdownWithReschedulingTimer) {
  CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  RunAndReschedule(cq, /*ok=*/true, std::chrono::seconds(1));

  cq.CancelAll();
  cq.Shutdown();
  t.join();
  // This is a "neither crashed nor hung" test. Reaching this point is success.
  SUCCEED();
}

TEST(CompletionQueueTest, CancelTimerSimple) {
  CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  using ms = std::chrono::milliseconds;
  auto fut = cq.MakeRelativeTimer(ms(20000));
  fut.cancel();
  auto tp = fut.get();
  EXPECT_FALSE(tp.ok()) << ", status=" << tp.status();
  cq.Shutdown();
  t.join();
}

TEST(CompletionQueueTest, CancelTimerContinuation) {
  CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  using ms = std::chrono::milliseconds;
  auto fut = cq.MakeRelativeTimer(ms(20000)).then(
      [](future<StatusOr<std::chrono::system_clock::time_point>> f) {
        return f.get().status();
      });
  fut.cancel();
  auto status = fut.get();
  EXPECT_FALSE(status.ok()) << ", status=" << status;
  cq.Shutdown();
  t.join();
}

TEST(CompletionQueueTest, ImplStartOperationDuplicate) {
  auto impl = std::make_shared<internal::DefaultCompletionQueueImpl>();
  auto op = std::make_shared<MockOperation>();

  impl->StartOperation(op, [](void*) {});

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        impl->StartOperation(op, [](void*) {});
      } catch (std::exception const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("duplicate operation"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(impl->StartOperation(op, [](void*) {}),
                            "duplicate operation");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
