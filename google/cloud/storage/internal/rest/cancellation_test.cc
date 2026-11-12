// Copyright 2026 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/credentials.h"
#include "google/cloud/future.h"
#include "google/cloud/rest_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::experimental::RestCancellation;
using ::google::cloud::experimental::RestCancellationOption;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using namespace std::chrono_literals;

TEST(RestCancellation, CopiesAndConcurrentCancel) {
  RestCancellation cancel;
  RestCancellation copy = cancel;
  RestCancellation independent;
  EXPECT_FALSE(copy.IsCancelled());
  std::vector<std::thread> threads;
  for (int i = 0; i != 8; ++i) {
    threads.emplace_back([copy] {
      for (int n = 0; n != 1000; ++n) {
        copy.Cancel();
        EXPECT_TRUE(copy.IsCancelled());
      }
    });
  }
  for (auto& t : threads) t.join();
  EXPECT_TRUE(cancel.IsCancelled());
  EXPECT_FALSE(independent.IsCancelled());
}

#ifndef _WIN32
// These tests use real loopback sockets, not mocked transport calls. The server
// signals after receiving headers and holds its response until test teardown.
// Timeouts are failure bounds; no sleep controls the ordering of cancellation.
class LocalServer {
 public:
  enum class Mode { kHeaders, kDownload, kUpload, kSuccess, kRetry };

  explicit LocalServer(Mode mode) : mode_(mode) {
    listener_ = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(listener_, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    EXPECT_EQ(
        bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)),
        0);
    socklen_t size = sizeof(address);
    EXPECT_EQ(
        getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &size),
        0);
    endpoint_ = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port));
    EXPECT_EQ(listen(listener_, 16), 0);
    thread_ = std::thread([this] { Accept(); });
  }
  ~LocalServer() { Stop(); }

  std::string const& endpoint() const { return endpoint_; }

  bool WaitForRequests(std::size_t count) {
    std::unique_lock<std::mutex> lk(mu_);
    return cv_.wait_for(lk, 5s, [&] { return requests_.size() >= count; });
  }
  std::vector<std::string> requests() {
    std::lock_guard<std::mutex> lk(mu_);
    return requests_;
  }
  void Stop() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (stopped_.exchange(true)) return;
    }
    cv_.notify_all();
    thread_.join();
    close(listener_);
    for (auto& t : workers_) t.join();
  }

 private:
  void Accept() {
    while (!stopped_) {
      pollfd p{listener_, POLLIN, 0};
      if (poll(&p, 1, 50) <= 0) continue;
      int const fd = accept(listener_, nullptr, nullptr);
      if (fd < 0) continue;
      workers_.emplace_back([this, fd] {
        Handle(fd);
        close(fd);
      });
    }
  }
  void Send(int fd, std::string const& data) {
    std::size_t offset = 0;
    while (offset != data.size()) {
#ifdef MSG_NOSIGNAL
      int const flags = MSG_NOSIGNAL;
#else
      int const flags = 0;
#endif
      auto const n =
          send(fd, data.data() + offset, data.size() - offset, flags);
      if (n <= 0) return;
      offset += static_cast<std::size_t>(n);
    }
  }
  void Handle(int fd) {
    // Bound failure cleanup, and suppress SIGPIPE without a process-wide
    // handler.
    timeval timeout{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
    int const one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    std::string request;
    while (request.find("\r\n\r\n") == std::string::npos) {
      char c;
      if (recv(fd, &c, 1, 0) != 1) return;
      request += c;
    }
    {
      std::lock_guard<std::mutex> lk(mu_);
      requests_.push_back(request);
    }
    cv_.notify_all();
    if (request.compare(0, 5, "POST ") == 0 && mode_ != Mode::kHeaders) {
      Send(fd, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nLocation: " +
                   endpoint_ + "/session\r\nConnection: close\r\n\r\n");
      return;
    }
    if (mode_ == Mode::kSuccess &&
        request.find("alt=media") != std::string::npos) {
      Send(fd,
           "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: "
           "close\r\n\r\ndata");
      return;
    }
    if (mode_ == Mode::kSuccess || request.find("/ok") != std::string::npos) {
      std::string const body = R"({"bucket":"bucket","name":"ok","size":"0"})";
      Send(fd,
           "HTTP/1.1 200 OK\r\nContent-Type: "
           "application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" +
               body);
      return;
    }
    if (mode_ == Mode::kRetry) {
      Send(fd,
           "HTTP/1.1 503 Unavailable\r\nContent-Length: 0\r\nConnection: "
           "close\r\n\r\n");
      return;
    }
    if (mode_ == Mode::kDownload) {
      Send(fd, "HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\n\r\n" +
                   std::string(128 * 1024, 'x'));
    }
    // For uploads, leave the body unread too: a sufficiently large PUT blocks
    // sending data, while a small final PUT blocks receiving its response.
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return stopped_.load(); });
  }

  Mode mode_;
  int listener_ = -1;
  std::string endpoint_;
  std::atomic<bool> stopped_{false};
  std::mutex mu_;
  std::condition_variable cv_;
  std::vector<std::string> requests_;
  std::thread thread_;
  std::vector<std::thread> workers_;
};

// Even a policy that retries cancellation must not cause a second request.
class RetryEverything : public RetryPolicy {
 public:
  std::unique_ptr<RetryPolicy> clone() const override {
    return std::make_unique<RetryEverything>();
  }
  bool OnFailure(Status const&) override { return ++failures_ < 3; }
  bool IsExhausted() const override { return failures_ >= 3; }
  bool IsPermanentFailure(Status const&) const override { return false; }

 private:
  int failures_ = 0;
};

Client MakeClient(LocalServer const& server) {
  return Client(
      Options{}
          .set<RestEndpointOption>(server.endpoint())
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
          .set<RetryPolicyOption>(std::make_shared<RetryEverything>())
          .set<BackoffPolicyOption>(
              std::make_shared<ExponentialBackoffPolicy>(1ms, 1ms, 2.0)));
}

// Join all request threads, including on a failure to cancel promptly.
// Releasing the server is deliberately after the readiness assertion, so server
// teardown cannot make a broken cancellation implementation pass.
class RequestThread {
 public:
  RequestThread(LocalServer& server, std::function<Status()> call)
      : server_(server),
        result_(promise_.get_future()),
        thread_(
            [this, call = std::move(call)] { promise_.set_value(call()); }) {}
  ~RequestThread() {
    server_.Stop();
    if (thread_.joinable()) thread_.join();
  }
  void ExpectBlocked() {
    EXPECT_EQ(result_.wait_for(50ms), std::future_status::timeout);
  }
  Status Finish() {
    EXPECT_EQ(result_.wait_for(500ms), std::future_status::ready);
    server_.Stop();
    thread_.join();
    return result_.get();
  }

 private:
  LocalServer& server_;
  promise<Status> promise_;
  future<Status> result_;
  std::thread thread_;
};

class SignallingBackoff : public BackoffPolicy {
 public:
  explicit SignallingBackoff(std::shared_ptr<promise<void>> entered)
      : entered_(std::move(entered)) {}
  std::unique_ptr<BackoffPolicy> clone() const override {
    return std::make_unique<SignallingBackoff>(entered_);
  }
  std::chrono::milliseconds OnCompletion() override {
    entered_->set_value();
    return 10s;
  }

 private:
  std::shared_ptr<promise<void>> entered_;
};

TEST(RestCancellation, InterruptsRetryBackoff) {
  LocalServer server(LocalServer::Mode::kRetry);
  Client client = MakeClient(server);
  RestCancellation cancel;
  auto entered = std::make_shared<promise<void>>();
  future<void> ready = entered->get_future();
  RequestThread request(server, [&] {
    return client
        .GetObjectMetadata(
            "bucket", "blocked",
            Options{}
                .set<RestCancellationOption>(cancel)
                .set<BackoffPolicyOption>(
                    std::make_shared<SignallingBackoff>(entered)))
        .status();
  });
  ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
  request.ExpectBlocked();
  cancel.Cancel();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
}

// Model a retry deadline expiring while the operation is in backoff, without
// relying on wall-clock timing to order expiration and cancellation.
class ExpirableRetryPolicy : public RetryEverything {
 public:
  explicit ExpirableRetryPolicy(std::shared_ptr<std::atomic<bool>> expired)
      : expired_(std::move(expired)) {}
  std::unique_ptr<RetryPolicy> clone() const override {
    return std::make_unique<ExpirableRetryPolicy>(expired_);
  }
  bool IsExhausted() const override {
    return expired_->load() || RetryEverything::IsExhausted();
  }

 private:
  std::shared_ptr<std::atomic<bool>> expired_;
};

class BackoffCancellation : public ::testing::TestWithParam<bool> {};
TEST_P(BackoffCancellation, CancelAfterRetryExpiration) {
  LocalServer server(LocalServer::Mode::kRetry);
  Client client = MakeClient(server);
  RestCancellation cancel;
  auto expired = std::make_shared<std::atomic<bool>>(false);
  auto entered = std::make_shared<promise<void>>();
  future<void> ready = entered->get_future();
  bool const upload = GetParam();
  Options const options =
      Options{}
          .set<RestCancellationOption>(cancel)
          .set<RetryPolicyOption>(
              std::make_shared<ExpirableRetryPolicy>(expired))
          .set<BackoffPolicyOption>(
              std::make_shared<SignallingBackoff>(entered));
  RequestThread request(server, [&] {
    if (!upload) {
      return client.GetObjectMetadata("bucket", "blocked", options).status();
    }
    auto stream = client.WriteObject("bucket", "blocked", options);
    stream << "partial object";
    stream.Close();
    return stream.metadata().status();
  });
  ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
  request.ExpectBlocked();
  expired->store(true);
  cancel.Cancel();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(upload ? 2 : 1));
}
INSTANTIATE_TEST_SUITE_P(RestCancellation, BackoffCancellation,
                         ::testing::Bool());

TEST(RestCancellation, BeforeStart) {
  LocalServer server(LocalServer::Mode::kSuccess);
  Client client = MakeClient(server);
  RestCancellation cancel;
  cancel.Cancel();
  Options const opts = Options{}.set<RestCancellationOption>(cancel);
  EXPECT_THAT(client.GetObjectMetadata("bucket", "blocked", opts),
              StatusIs(StatusCode::kCancelled));
  auto read = client.ReadObject("bucket", "blocked", opts);
  EXPECT_THAT(read.status(), StatusIs(StatusCode::kCancelled));
  auto write = client.WriteObject("bucket", "blocked", opts);
  write.Close();
  EXPECT_THAT(write.metadata(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::IsEmpty());
  EXPECT_THAT(client.GetObjectMetadata("bucket", "ok"), IsOk());
}

TEST(RestCancellation, WaitingForHeadersAndIsolation) {
  LocalServer server(LocalServer::Mode::kHeaders);
  Client client = MakeClient(server);
  RestCancellation cancel;
  RequestThread request(server, [&] {
    return client
        .GetObjectMetadata("bucket", "blocked",
                           Options{}.set<RestCancellationOption>(cancel))
        .status();
  });
  ASSERT_TRUE(server.WaitForRequests(1));
  // Same client, concurrent request, no shared cancellation state.
  EXPECT_THAT(client.GetObjectMetadata("bucket", "ok"), IsOk());
  request.ExpectBlocked();
  cancel.Cancel();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(2));
}

TEST(RestCancellation, SaturatedHedgingPoolsDoNotDelayCancellation) {
  LocalServer server(LocalServer::Mode::kHeaders);
  Client client(
      Options{}
          .set<RestEndpointOption>(server.endpoint())
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(0).clone())
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::ReadThreadPoolSizeOption>(1)
          .set<storage_experimental::HedgingThreadPoolSizeOption>(1)
          .set<storage_experimental::MaxReadHedgesOption>(1)
          .set<storage_experimental::ReadHedgeDelayOption>(0ms));
  RequestThread occupying(server, [&] {
    auto stream = client.ReadObject("bucket", "occupying");
    return stream.status();
  });
  // The uncancellable read occupies both the primary and hedge worker. Neither
  // can finish until the server is released during test cleanup.
  ASSERT_TRUE(server.WaitForRequests(2));
  occupying.ExpectBlocked();

  RestCancellation cancel;
  RequestThread request(server, [&] {
    auto stream = client.ReadObject(
        "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
    return stream.status();
  });
  EXPECT_TRUE(server.WaitForRequests(3));
  request.ExpectBlocked();
  cancel.Cancel();
  // Finish requires completion before releasing the server and its occupied
  // workers. A cancellable read must therefore execute without queueing there.
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(3));
}

TEST(RestCancellation, OpeningDownload) {
  LocalServer server(LocalServer::Mode::kHeaders);
  Client client = MakeClient(server);
  RestCancellation cancel;
  RequestThread request(server, [&] {
    auto stream = client.ReadObject(
        "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
    return stream.status();
  });
  ASSERT_TRUE(server.WaitForRequests(1));
  request.ExpectBlocked();
  cancel.Cancel();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
}

TEST(RestCancellation, StalledDownload) {
  LocalServer server(LocalServer::Mode::kDownload);
  Client client = MakeClient(server);
  RestCancellation cancel;
  promise<void> opened;
  future<void> ready = opened.get_future();
  RequestThread request(server, [&] {
    auto stream = client.ReadObject(
        "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
    opened.set_value();
    std::vector<char> data(1024 * 1024);
    stream.read(data.data(), static_cast<std::streamsize>(data.size()));
    return stream.status();
  });
  ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
  request.ExpectBlocked();
  cancel.Cancel();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
}

TEST(RestCancellation, CancelledDownloadCleanupDoesNotDrain) {
  LocalServer server(LocalServer::Mode::kDownload);
  Client client = MakeClient(server);
  RestCancellation cancel;
  promise<void> opened;
  future<void> ready = opened.get_future();
  promise<void> destroy;
  future<void> proceed = destroy.get_future();
  RequestThread request(server, [&] {
    {
      auto stream = client.ReadObject(
          "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
      opened.set_value();
      // Bound failure cleanup even if opening the stream fails.
      (void)proceed.wait_for(5s);
    }
    return Status{};
  });
  ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
  cancel.Cancel();
  destroy.set_value();
  EXPECT_THAT(request.Finish(), IsOk());
  EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
}

TEST(RestCancellation, ClosingCancelledDownloadReportsCancellation) {
  LocalServer server(LocalServer::Mode::kDownload);
  Client client = MakeClient(server);
  RestCancellation cancel;
  promise<void> opened;
  future<void> ready = opened.get_future();
  promise<void> close;
  future<void> proceed = close.get_future();
  RequestThread request(server, [&] {
    auto stream = client.ReadObject(
        "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
    opened.set_value();
    // Bound failure cleanup even if opening the stream fails.
    (void)proceed.wait_for(5s);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    EXPECT_THROW(stream.Close(), google::cloud::RuntimeStatusError);
#else
    stream.Close();
    EXPECT_TRUE(stream.bad());
#endif
    EXPECT_FALSE(stream.IsOpen());
    return stream.status();
  });
  ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
  cancel.Cancel();
  close.set_value();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
}

TEST(RestCancellation, OpeningUpload) {
  LocalServer server(LocalServer::Mode::kHeaders);
  Client client = MakeClient(server);
  RestCancellation cancel;
  RequestThread request(server, [&] {
    auto stream = client.WriteObject(
        "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
    stream.Close();
    return stream.metadata().status();
  });
  ASSERT_TRUE(server.WaitForRequests(1));
  request.ExpectBlocked();
  cancel.Cancel();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
}

class UploadCancellation : public ::testing::TestWithParam<bool> {};
TEST_P(UploadCancellation, BlockedWriteOrFinalization) {
  LocalServer server(LocalServer::Mode::kUpload);
  Client client = MakeClient(server);
  RestCancellation cancel;
  bool const finalizing = GetParam();
  RequestThread request(server, [&] {
    auto stream = client.WriteObject(
        "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
    if (finalizing) {
      stream << "partial object";
    } else {
      std::string const data(32 * 1024 * 1024, 'x');
      stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    stream.Close();
    return stream.metadata().status();
  });
  ASSERT_TRUE(server.WaitForRequests(2));
  request.ExpectBlocked();
  cancel.Cancel();
  EXPECT_THAT(request.Finish(), StatusIs(StatusCode::kCancelled));
  auto const requests = server.requests();
  ASSERT_THAT(requests, ::testing::SizeIs(2));
  EXPECT_THAT(requests[1], HasSubstr("PUT /session"));
  if (!finalizing) EXPECT_THAT(requests[1], HasSubstr("/*"));
}
INSTANTIATE_TEST_SUITE_P(RestCancellation, UploadCancellation,
                         ::testing::Bool());

TEST(RestCancellation, CleanupDoesNotFinalizeBufferedUpload) {
  LocalServer server(LocalServer::Mode::kUpload);
  Client client = MakeClient(server);
  RestCancellation cancel;
  {
    auto stream = client.WriteObject(
        "bucket", "blocked", Options{}.set<RestCancellationOption>(cancel));
    stream << "partial object";
    cancel.Cancel();
    // Destructor would normally send the final chunk.
  }
  EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
  EXPECT_THAT(client.GetObjectMetadata("bucket", "ok"), IsOk());
}

TEST(RestCancellation, OrdinaryStreamsSucceed) {
  LocalServer server(LocalServer::Mode::kSuccess);
  Client client = MakeClient(server);
  RestCancellation cancel;
  for (Options const& opts :
       {Options{}, Options{}.set<RestCancellationOption>(cancel)}) {
    auto read = client.ReadObject("bucket", "object", opts);
    std::string contents(std::istreambuf_iterator<char>{read}, {});
    EXPECT_THAT(contents, Eq("data"));
    EXPECT_THAT(read.status(), IsOk());
    auto write = client.WriteObject("bucket", "object", opts);
    write << "data";
    write.Close();
    EXPECT_THAT(write.metadata(), IsOk());
  }
}

TEST(RestCancellation, RaceWithCompletion) {
  for (int i = 0; i != 20; ++i) {
    LocalServer server(LocalServer::Mode::kSuccess);
    Client client = MakeClient(server);
    RestCancellation cancel;
    RequestThread request(server, [&] {
      return client
          .GetObjectMetadata("bucket", "ok",
                             Options{}.set<RestCancellationOption>(cancel))
          .status();
    });
    ASSERT_TRUE(server.WaitForRequests(1));
    std::thread canceller([cancel] { cancel.Cancel(); });
    Status const result = request.Finish();
    canceller.join();
    EXPECT_THAT(result,
                ::testing::AnyOf(IsOk(), StatusIs(StatusCode::kCancelled)));
    EXPECT_THAT(server.requests(), ::testing::SizeIs(1));
  }
}

TEST(RestCancellation, CompletedRequestAndHandleReuse) {
  LocalServer server(LocalServer::Mode::kSuccess);
  Client client = MakeClient(server);
  for (int i = 0; i != 20; ++i) {
    RestCancellation cancel;
    EXPECT_THAT(
        client.GetObjectMetadata("bucket", "ok",
                                 Options{}.set<RestCancellationOption>(cancel)),
        IsOk());
    cancel.Cancel();
    EXPECT_THAT(client.GetObjectMetadata("bucket", "ok"), IsOk());
  }
}
#endif  // !_WIN32

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
