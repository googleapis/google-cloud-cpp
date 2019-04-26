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

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <gmock/gmock.h>
#include <future>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace btproto = google::bigtable::v2;
using namespace google::cloud::testing_util::chrono_literals;

/**
 * Implement a single streaming read RPC to test the wrappers.
 */
class BulkApplyImpl final : public google::bigtable::v2::Bigtable::Service {
 public:
  BulkApplyImpl() = default;

  grpc::Status MutateRows(
      grpc::ServerContext* context,
      google::bigtable::v2::MutateRowsRequest const* request,
      grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>* writer)
      override {
    std::unique_lock<std::mutex> lk(mu_);
    if (!callback_) {
      return grpc::Status::OK;
    }
    Callback cb;
    cb.swap(callback_);
    lk.unlock();
    return cb(context, request, writer);
  }

  using Callback = std::function<grpc::Status(
      grpc::ServerContext*, google::bigtable::v2::MutateRowsRequest const*,
      grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*)>;

  void SetCallback(Callback callback) {
    std::unique_lock<std::mutex> lk(mu_);
    callback_ = std::move(callback);
  }

 private:
  std::mutex mu_;
  Callback callback_;
};

/**
 * This test starts a server in a separate thread, and then executes against
 * that server. We want to test the wrappers end-to-end, particularly with
 * respect to error handling, and cancellation.
 */
class AsyncReadStreamTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int port;
    std::string server_address("[::]:0");
    builder_.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                              &port);
    builder_.RegisterService(&impl_);
    server_ = builder_.BuildAndStart();
    server_thread_ = std::thread([this]() { server_->Wait(); });

    std::shared_ptr<grpc::Channel> channel =
        grpc::CreateChannel("localhost:" + std::to_string(port),
                            grpc::InsecureChannelCredentials());
    stub_ = google::bigtable::v2::Bigtable::NewStub(channel);

    cq_thread_ = std::thread([this] { cq_.Run(); });
  }

  void TearDown() override {
    cq_.Shutdown();
    if (cq_thread_.joinable()) {
      cq_thread_.join();
    }
    WaitForServerShutdown();
  }

  void WaitForServerShutdown() {
    server_->Shutdown();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  void WriteOne(
      grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>* writer,
      int index) {
    google::bigtable::v2::MutateRowsResponse response;
    response.add_entries()->set_index(index);
    writer->Write(response, grpc::WriteOptions().set_write_through());
  }

  void WriteLast(
      grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>* writer,
      int index) {
    google::bigtable::v2::MutateRowsResponse response;
    response.add_entries()->set_index(index);
    writer->Write(response,
                  grpc::WriteOptions().set_write_through().set_last_message());
  }

  BulkApplyImpl impl_;
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Server> server_;
  std::thread server_thread_;
  std::unique_ptr<google::bigtable::v2::Bigtable::StubInterface> stub_;

  CompletionQueue cq_;
  std::thread cq_thread_;
};

// A synchronization primitive to block a thread until it is allowed to
// continue.
class SimpleBarrier {
 public:
  SimpleBarrier() = default;

  void Wait() { impl_.get_future().get(); }
  void Lift() { impl_.set_value(); }

 private:
  promise<void> impl_;
};

struct HandlerResult {
  std::vector<btproto::MutateRowsResponse> reads;
  Status status;
  SimpleBarrier done;
};

/// @test Verify that completion queues correctly validate asynchronous
/// streaming read RPC callables.
TEST_F(AsyncReadStreamTest, MetaFunctions) {
  auto async_call = [this](grpc::ClientContext* context,
                           btproto::MutateRowsRequest const& request,
                           grpc::CompletionQueue* cq) {
    return stub_->PrepareAsyncMutateRows(context, request, cq);
  };
  static_assert(
      std::is_same<
          btproto::MutateRowsResponse,
          internal::AsyncStreamingReadResponseType<
              decltype(async_call), btproto::MutateRowsRequest>::type>::value,
      "Unexpected type for AsyncStreamingReadResponseType<>");
}

/// @test Verify that AsyncReadStream works even if the server does not exist.
TEST_F(AsyncReadStreamTest, CannotConnect) {
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("localhost:0", grpc::InsecureChannelCredentials());
  std::unique_ptr<google::bigtable::v2::Bigtable::StubInterface> stub =
      google::bigtable::v2::Bigtable::NewStub(channel);

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  cq_.MakeStreamingReadRpc(
      [&stub](grpc::ClientContext* context,
              btproto::MutateRowsRequest const& request,
              grpc::CompletionQueue* cq) {
        return stub->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        return make_ready_future(true);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  EXPECT_TRUE(result.reads.empty());
  EXPECT_EQ(StatusCode::kUnavailable, result.status.code());
}

/// @test Verify that the AsyncReadStream handles an empty stream.
TEST_F(AsyncReadStreamTest, Empty) {
  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        return make_ready_future(true);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  EXPECT_TRUE(result.reads.empty());
  EXPECT_STATUS_OK(result.status);
}

/// @test Verify that the AsyncReadStream handles an error in a empty stream.
TEST_F(AsyncReadStreamTest, FailImmediately) {
  impl_.SetCallback(
      [](grpc::ServerContext*, google::bigtable::v2::MutateRowsRequest const*,
         grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh");
      });

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        return make_ready_future(true);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  EXPECT_TRUE(result.reads.empty());
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status.code());
}

/// @test Verify that the AsyncReadStream handles a stream with 3 elements.
TEST_F(AsyncReadStreamTest, Return3) {
  impl_.SetCallback(
      [this](grpc::ServerContext*,
             google::bigtable::v2::MutateRowsRequest const*,
             grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
                 writer) {
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        WriteLast(writer, 2);
        return grpc::Status::OK;
      });

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        return make_ready_future(true);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  EXPECT_STATUS_OK(result.status);
  ASSERT_EQ(3U, result.reads.size());
  for (int i = 0; i != 3; ++i) {
    SCOPED_TRACE("Running iteration: " + std::to_string(i));
    ASSERT_EQ(1, result.reads[i].entries_size());
    EXPECT_EQ(i, result.reads[i].entries(0).index());
  }
}

/// @test Verify that the AsyncReadStream detect errors reported by the server.
TEST_F(AsyncReadStreamTest, Return3ThenFail) {
  // Very rarely (in the CI builds, with high load), all 3 responses and the
  // error message are coalesced into a single message from the server, and then
  // the OnRead() calls do not happen. We need to explicitly synchronize the
  // client and server threads.
  SimpleBarrier server_barrier;
  impl_.SetCallback(
      [this, &server_barrier](
          grpc::ServerContext*, google::bigtable::v2::MutateRowsRequest const*,
          grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
              writer) {
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        // Cannot use WriteLast() because that blocks until the status is
        // returned, and we want to pause in `server_barrier` to ensure all
        // messages are received.
        WriteOne(writer, 2);
        // Block until the client has received the responses.
        server_barrier.Wait();
        return grpc::Status(grpc::StatusCode::INTERNAL, "bad luck");
      });

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result, &server_barrier](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        if (result.reads.size() == 3U) {
          server_barrier.Lift();
        }
        return make_ready_future(true);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  ASSERT_EQ(3U, result.reads.size());
  for (int i = 0; i != 3; ++i) {
    SCOPED_TRACE("Running iteration: " + std::to_string(i));
    ASSERT_EQ(1, result.reads[i].entries_size());
    EXPECT_EQ(i, result.reads[i].entries(0).index());
  }
  EXPECT_EQ(StatusCode::kInternal, result.status.code());
}

/// @test Verify that the AsyncReadStream wrappers work even if the server does
/// not explicitly signal end-of-stream.
TEST_F(AsyncReadStreamTest, Return3NoLast) {
  impl_.SetCallback(
      [this](grpc::ServerContext*,
             google::bigtable::v2::MutateRowsRequest const*,
             grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
                 writer) {
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        WriteOne(writer, 2);
        return grpc::Status::OK;
      });

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        return make_ready_future(true);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  ASSERT_EQ(3U, result.reads.size());
  ASSERT_STATUS_OK(result.status);
  for (int i = 0; i != 3; ++i) {
    SCOPED_TRACE("Running iteration: " + std::to_string(i));
    ASSERT_EQ(1, result.reads[i].entries_size());
    EXPECT_EQ(i, result.reads[i].entries(0).index());
  }
}

/// @test Verify that the AsyncReadStream wrappers work even if the last Read()
/// blocks for a bit.
TEST_F(AsyncReadStreamTest, Return3LastIsBlocked) {
  SimpleBarrier client_barrier;
  SimpleBarrier server_barrier;
  impl_.SetCallback(
      [this, &client_barrier, &server_barrier](
          grpc::ServerContext*, google::bigtable::v2::MutateRowsRequest const*,
          grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
              writer) {
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        WriteOne(writer, 2);
        client_barrier.Lift();
        server_barrier.Wait();
        return grpc::Status::OK;
      });

  HandlerResult result;
  auto on_read = [&client_barrier, &server_barrier,
                  &result](btproto::MutateRowsResponse r) {
    result.reads.emplace_back(std::move(r));
    if (result.reads.size() == 3U) {
      client_barrier.Wait();
      server_barrier.Lift();
    }
    return make_ready_future(true);
  };

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context), on_read,
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  ASSERT_EQ(3U, result.reads.size());
  ASSERT_STATUS_OK(result.status);
  for (int i = 0; i != 3; ++i) {
    SCOPED_TRACE("Running iteration: " + std::to_string(i));
    ASSERT_EQ(1, result.reads[i].entries_size());
    EXPECT_EQ(i, result.reads[i].entries(0).index());
  }
}

/// @test Verify that AsyncReadStream::Cancel() works in the middle of a Read().
TEST_F(AsyncReadStreamTest, CancelWhileBlocked) {
  SimpleBarrier client_barrier;
  SimpleBarrier server_barrier;
  impl_.SetCallback(
      [this, &client_barrier, &server_barrier](
          grpc::ServerContext*, google::bigtable::v2::MutateRowsRequest const*,
          grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
              writer) {
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        client_barrier.Lift();
        server_barrier.Wait();
        WriteOne(writer, 2);
        return grpc::Status::OK;
      });

  HandlerResult result;
  auto on_read = [&client_barrier, &result](btproto::MutateRowsResponse r) {
    result.reads.emplace_back(std::move(r));
    if (result.reads.size() == 2U) {
      client_barrier.Wait();
      return make_ready_future(false);
    }
    return make_ready_future(true);
  };

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context), on_read,
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  // The server remains blocked until the stream finishes, therefore, the only
  // way this actually unblocks is if the Cancel() succeeds.
  result.done.Wait();
  ASSERT_EQ(2U, result.reads.size());
  EXPECT_EQ(StatusCode::kCancelled, result.status.code());
  for (int i = 0; i != 2; ++i) {
    SCOPED_TRACE("Running iteration: " + std::to_string(i));
    ASSERT_EQ(1, result.reads[i].entries_size());
    EXPECT_EQ(i, result.reads[i].entries(0).index());
  }

  // The barriers go out of scope when this function exits, but the server may
  // still be using them, so wait for the server to shutdown before leaving the
  // scope.
  server_barrier.Lift();
  WaitForServerShutdown();
}

/// @test Verify that AsyncReadStream works when one calls Cancel() more than
/// once.
TEST_F(AsyncReadStreamTest, DoubleCancel) {
  SimpleBarrier server_sent_responses_barrier;
  SimpleBarrier cancel_done_server_barrier;
  impl_.SetCallback(
      [this, &server_sent_responses_barrier, &cancel_done_server_barrier](
          grpc::ServerContext*, google::bigtable::v2::MutateRowsRequest const*,
          grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
              writer) {
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        server_sent_responses_barrier.Lift();
        cancel_done_server_barrier.Wait();
        WriteOne(writer, 2);
        return grpc::Status::OK;
      });

  HandlerResult result;
  SimpleBarrier read_received_barrier;
  SimpleBarrier cancel_done_read_barrier;
  auto on_read = [&read_received_barrier, &cancel_done_read_barrier,
                  &result](btproto::MutateRowsResponse r) {
    result.reads.emplace_back(std::move(r));
    if (result.reads.size() == 2U) {
      read_received_barrier.Lift();
      cancel_done_read_barrier.Wait();
    }
    return make_ready_future(true);
  };

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  auto op = cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context), on_read,
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  server_sent_responses_barrier.Wait();
  read_received_barrier.Wait();
  op->Cancel();
  op->Cancel();
  cancel_done_server_barrier.Lift();
  cancel_done_read_barrier.Lift();

  // The server remains blocked until the stream finishes, therefore, the only
  // way this actually unblocks is if the Cancel() succeeds.
  result.done.Wait();
  ASSERT_EQ(2U, result.reads.size());
  EXPECT_EQ(StatusCode::kCancelled, result.status.code());
  for (int i = 0; i != 2; ++i) {
    SCOPED_TRACE("Running iteration: " + std::to_string(i));
    ASSERT_EQ(1, result.reads[i].entries_size());
    EXPECT_EQ(i, result.reads[i].entries(0).index());
  }

  // The barriers go out of scope when this function exits, but the server may
  // still be using them, so wait for the server to shutdown before leaving the
  // scope.
  WaitForServerShutdown();
}

/// @test Verify that AsyncReadStream works when one Cancels() before reading.
TEST_F(AsyncReadStreamTest, CancelBeforeRead) {
  SimpleBarrier server_started_barrier;
  SimpleBarrier cancel_done_server_barrier;
  impl_.SetCallback(
      [this, &server_started_barrier, &cancel_done_server_barrier](
          grpc::ServerContext*, google::bigtable::v2::MutateRowsRequest const*,
          grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
              writer) {
        server_started_barrier.Lift();
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        WriteOne(writer, 2);
        cancel_done_server_barrier.Wait();
        return grpc::Status::OK;
      });

  HandlerResult result;
  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  auto op = cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        return make_ready_future(true);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  server_started_barrier.Wait();
  op->Cancel();

  // The server remains blocked until the stream finishes, therefore, the only
  // way this actually unblocks is if the Cancel() succeeds.
  result.done.Wait();
  // There is no guarantee on how many messages will be received before the
  // cancel succeeds, but we certainly expect fewer messages than we sent.
  EXPECT_LE(result.reads.size(), 3U);
  EXPECT_EQ(StatusCode::kCancelled, result.status.code());

  // The barriers go out of scope when this function exits, but the server may
  // still be using them, so wait for the server to shutdown before leaving the
  // scope.
  cancel_done_server_barrier.Lift();
  WaitForServerShutdown();
}

/// @test Verify that AsyncReadStream works even if Cancel() is misused.
TEST_F(AsyncReadStreamTest, CancelAfterFinish) {
  impl_.SetCallback(
      [this](grpc::ServerContext*,
             google::bigtable::v2::MutateRowsRequest const*,
             grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
                 writer) {
        WriteOne(writer, 0);
        WriteOne(writer, 1);
        WriteLast(writer, 2);
        return grpc::Status::OK;
      });

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  SimpleBarrier on_finish_stop_before_cancel;
  SimpleBarrier on_finish_continue_after_cancel;
  auto on_finish = [&result, &on_finish_stop_before_cancel,
                    &on_finish_continue_after_cancel](Status s) {
    result.status = std::move(s);
    on_finish_stop_before_cancel.Lift();
    on_finish_continue_after_cancel.Wait();
    result.done.Lift();
  };
  auto op = cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        return make_ready_future(true);
      },
      on_finish);

  // Call Cancel() while the on_finish() callback is running.
  on_finish_stop_before_cancel.Wait();
  op->Cancel();
  on_finish_continue_after_cancel.Lift();

  result.done.Wait();
  EXPECT_STATUS_OK(result.status);
  ASSERT_EQ(3U, result.reads.size());
  for (int i = 0; i != 3; ++i) {
    SCOPED_TRACE("Running iteration: " + std::to_string(i));
    ASSERT_EQ(1, result.reads[i].entries_size());
    EXPECT_EQ(i, result.reads[i].entries(0).index());
  }
}

/// @test Verify that AsyncReadStream works when returning false from OnRead().
TEST_F(AsyncReadStreamTest, DiscardAfterReturningFalse) {
  impl_.SetCallback(
      [this](grpc::ServerContext*,
             google::bigtable::v2::MutateRowsRequest const*,
             grpc::ServerWriter<google::bigtable::v2::MutateRowsResponse>*
                 writer) {
        for (int i = 0; i != 10; ++i) {
          WriteOne(writer, i);
        }
        WriteLast(writer, 10);
        return grpc::Status::OK;
      });

  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  HandlerResult result;
  auto op = cq_.MakeStreamingReadRpc(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request,
             grpc::CompletionQueue* cq) {
        return stub_->PrepareAsyncMutateRows(context, request, cq);
      },
      request, std::move(context),
      [&result](btproto::MutateRowsResponse r) {
        result.reads.emplace_back(std::move(r));
        // Cancel on *every* request, we do not expect additional calls after
        // the first one.
        return make_ready_future(false);
      },
      [&result](Status s) {
        result.status = std::move(s);
        result.done.Lift();
      });

  result.done.Wait();
  ASSERT_EQ(StatusCode::kCancelled, result.status.code());
  ASSERT_EQ(1U, result.reads.size());
  EXPECT_EQ(1, result.reads[0].entries_size());
  EXPECT_EQ(0, result.reads[0].entries(0).index());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
