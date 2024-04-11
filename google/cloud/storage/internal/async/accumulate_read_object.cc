// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/accumulate_read_object.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/make_cord.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/internal/make_status.h"
#include <iterator>
#include <numeric>
#include <sstream>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/**
 * Keeps the state for `AsyncAccumulateReadObjectPartial()`.
 *
 * This is analogous to a C++20 coroutine handle. It keeps the state for
 * `AsyncAccumulateReadObjectPartial()`, that is, all the function call
 * arguments, as well as any local variables.
 *
 * Whereas in C++20 coroutines we would use `co_await` to suspend execution,
 * here we need to use a callback, so code like:
 *
 * @code
 *   auto x = co_await Foo();
 * @endcode
 *
 * becomes
 *
 * @code
 *   Foo().then([self = shared_from_this()](auto f) { self->OnFoo(f.get()); });
 * @endcode
 *
 * An extra complication is that we use timeouts, so while a naive
 * implementation of this code would say:
 *
 * @code
 *   auto read = co_await stream->Read();
 * @endcode
 *
 * here we launch two coroutines, one to run the timer, and one to actually
 * make the call subject to a timeout. The trick is to set the timeout to cancel
 * the full streaming RPC if it expires successfully, and to cancel the timer
 * if the operation completes:
 *
 * @code
 *   auto tm = cq.MakeRelativeTimer(timeout).then([&stream](auto f) {
 *     if (!f.get().ok()) return false; // timer canceled, nothing to do
 *     stream->Cancel();
 *     return true;
 *   });
 *   auto read = co_await stream->Read();
 *   tm.cancel();
 *   if (co_await tm) { // handle the timeout
 *   // success!
 * @endcode
 *
 * Without coroutines this requires passing the timer future to the `OnRead()`
 * callback. See below for the details.
 */
class AsyncAccumulateReadObjectPartialHandle
    : public std::enable_shared_from_this<
          AsyncAccumulateReadObjectPartialHandle> {
 public:
  using Response = ::google::storage::v2::ReadObjectResponse;
  using Stream = ::google::cloud::internal::AsyncStreamingReadRpc<Response>;
  using RpcMetadata = ::google::cloud::RpcMetadata;
  using Result = AsyncAccumulateReadObjectResult;

  AsyncAccumulateReadObjectPartialHandle(CompletionQueue cq,
                                         std::unique_ptr<Stream> stream,
                                         std::chrono::milliseconds timeout)
      : cq_(std::move(cq)), stream_(std::move(stream)), timeout_(timeout) {}

  future<Result> Invoke() {
    struct ByMove {
      std::shared_ptr<AsyncAccumulateReadObjectPartialHandle> self;
      future<bool> tm;
      void operator()(future<bool> f) { self->OnStart(std::move(tm), f.get()); }
    };
    auto tm = MakeTimeout();
    stream_->Start().then(ByMove{shared_from_this(), std::move(tm)});
    return promise_.get_future();
  }

 private:
  void OnStart(future<bool> tm, bool read_ok) {
    tm.cancel();
    auto self = shared_from_this();
    tm.then([self, read_ok](future<bool> f) {
      self->OnStartTimer(f.get(), read_ok);
    });
  }

  void OnStartTimer(bool timer_ok, bool read_ok) {
    if (timer_ok) return OnTimeout("Start()");
    if (!read_ok) return Finish();
    Read();
  }

  void Read() {
    struct ByMove {
      std::shared_ptr<AsyncAccumulateReadObjectPartialHandle> self;
      future<bool> tm;
      void operator()(future<absl::optional<Response>> f) {
        self->OnRead(std::move(tm), f.get());
      }
    };
    auto tm = MakeTimeout();
    stream_->Read().then(ByMove{shared_from_this(), std::move(tm)});
  }

  void OnRead(future<bool> tm, absl::optional<Response> response) {
    tm.cancel();
    struct ByMove {
      std::shared_ptr<AsyncAccumulateReadObjectPartialHandle> self;
      absl::optional<Response> response;
      void operator()(future<bool> f) {
        self->OnReadTimer(f.get(), std::move(response));
      }
    };
    tm.then(ByMove{shared_from_this(), std::move(response)});
  }

  void OnReadTimer(bool timer_ok, absl::optional<Response> response) {
    if (timer_ok) return OnTimeout("Read()");
    if (!response.has_value()) return Finish();
    accumulator_.push_back(*std::move(response));
    Read();
  }

  void Finish() {
    struct ByMove {
      std::shared_ptr<AsyncAccumulateReadObjectPartialHandle> self;
      future<bool> tm;
      void operator()(future<Status> f) {
        self->OnFinish(std::move(tm), f.get());
      }
    };
    auto tm = MakeTimeout();
    stream_->Finish().then(ByMove{shared_from_this(), std::move(tm)});
  }

  void OnFinish(future<bool> tm, Status status) {
    tm.cancel();
    promise_.set_value(Result{std::move(accumulator_),
                              stream_->GetRequestMetadata(),
                              std::move(status)});
  }

  future<bool> MakeTimeout() {
    auto self = shared_from_this();
    return cq_.MakeRelativeTimer(timeout_).then(
        [self](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          if (!f.get().ok()) return false;
          self->stream_->Cancel();
          return true;
        });
  }

  void OnTimeout(char const* where) {
    auto finish = stream_->Finish();
    finish.then(WaitForFinish{std::move(stream_)});
    promise_.set_value(
        Result{std::move(accumulator_), RpcMetadata{},
               Status(StatusCode::kDeadlineExceeded,
                      std::string{"Timeout waiting for "} + where)});
  }

  // Assume ownership of `stream` until its `Finish()` callback completes.
  struct WaitForFinish {
    std::unique_ptr<Stream> stream;
    void operator()(future<Status>) const {}
  };

  promise<Result> promise_;
  std::vector<Response> accumulator_;
  CompletionQueue cq_;
  std::unique_ptr<Stream> stream_;
  std::chrono::milliseconds timeout_;
};

class AsyncAccumulateReadObjectFullHandle
    : public std::enable_shared_from_this<AsyncAccumulateReadObjectFullHandle> {
 public:
  AsyncAccumulateReadObjectFullHandle(
      CompletionQueue cq, std::shared_ptr<StorageStub> stub,
      std::function<std::shared_ptr<grpc::ClientContext>()> context_factory,
      google::storage::v2::ReadObjectRequest request,
      google::cloud::internal::ImmutableOptions options)
      : cq_(std::move(cq)),
        stub_(std::move(stub)),
        context_factory_(std::move(context_factory)),
        request_(std::move(request)),
        timeout_(options->get<storage::DownloadStallTimeoutOption>()),
        retry_(options->get<storage::RetryPolicyOption>()->clone()),
        backoff_(options->get<storage::BackoffPolicyOption>()->clone()),
        options_(std::move(options)) {
    accumulator_.status = Status(StatusCode::kDeadlineExceeded,
                                 "retry policy exhausted before first request");
  }

  future<AsyncAccumulateReadObjectResult> Invoke() {
    Loop();
    return promise_.get_future();
  }

 private:
  void Loop() {
    if (retry_->IsExhausted()) {
      promise_.set_value(std::move(accumulator_));
      return;
    }
    auto self = shared_from_this();
    auto stream =
        stub_->AsyncReadObject(cq_, context_factory_(), options_, request_);
    AsyncAccumulateReadObjectPartial(cq_, std::move(stream), timeout_)
        .then([self](future<AsyncAccumulateReadObjectResult> f) {
          self->OnPartial(f.get());
        });
  }

  void OnPartial(AsyncAccumulateReadObjectResult partial) {
    auto const size = std::accumulate(
        partial.payload.begin(), partial.payload.end(), std::int64_t{0},
        [](std::int64_t a, google::storage::v2::ReadObjectResponse const& r) {
          if (!r.has_checksummed_data()) return a;
          auto const s = GetContent(r.checksummed_data()).size();
          return a + static_cast<std::int64_t>(s);
        });
    accumulator_.status = std::move(partial.status);
    accumulator_.payload.insert(
        accumulator_.payload.end(),
        std::make_move_iterator(partial.payload.begin()),
        std::make_move_iterator(partial.payload.end()));
    accumulator_.metadata.headers.insert(
        std::make_move_iterator(partial.metadata.headers.begin()),
        std::make_move_iterator(partial.metadata.headers.end()));
    accumulator_.metadata.trailers.insert(
        std::make_move_iterator(partial.metadata.trailers.begin()),
        std::make_move_iterator(partial.metadata.trailers.end()));
    // We need to make sure the next read is from the same object, not a new
    // version of the object we just read.
    auto const generation = [this] {
      for (auto const& r : accumulator_.payload) {
        if (!r.has_metadata()) continue;
        return r.metadata().generation();
      }
      return std::int64_t{0};
    }();
    if (request_.read_limit() != 0 && size > request_.read_limit()) {
      std::ostringstream os;
      os << "too many bytes returned in ReadObject(), expected="
         << request_.read_limit() << ", got=" << size;
      accumulator_.status = Status(StatusCode::kInternal, os.str());
      promise_.set_value(std::move(accumulator_));
      return;
    }
    if (accumulator_.status.ok() || !retry_->OnFailure(accumulator_.status)) {
      promise_.set_value(std::move(accumulator_));
      return;
    }
    request_.set_generation(generation);
    request_.set_read_offset(request_.read_offset() + size);
    request_.set_read_limit(
        request_.read_limit() == 0 ? 0 : request_.read_limit() - size);
    auto self = shared_from_this();
    cq_.MakeRelativeTimer(backoff_->OnCompletion())
        .then(
            [self](future<StatusOr<std::chrono::system_clock::time_point>> f) {
              self->OnBackoff(f.get());
            });
  }

  void OnBackoff(StatusOr<std::chrono::system_clock::time_point> timer) {
    if (!timer) {
      accumulator_.status = std::move(timer).status();
      promise_.set_value(std::move(accumulator_));
      return;
    }
    Loop();
  }

  promise<AsyncAccumulateReadObjectResult> promise_;
  AsyncAccumulateReadObjectResult accumulator_;
  CompletionQueue cq_;
  std::shared_ptr<StorageStub> stub_;
  std::function<std::shared_ptr<grpc::ClientContext>()> context_factory_;
  google::storage::v2::ReadObjectRequest request_;
  std::chrono::milliseconds timeout_;
  std::unique_ptr<storage::RetryPolicy> retry_;
  std::unique_ptr<storage::BackoffPolicy> backoff_;
  google::cloud::internal::ImmutableOptions options_;
};

}  // namespace

future<AsyncAccumulateReadObjectResult> AsyncAccumulateReadObjectPartial(
    CompletionQueue cq,
    std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
        google::storage::v2::ReadObjectResponse>>
        stream,
    std::chrono::milliseconds timeout) {
  auto handle = std::make_shared<AsyncAccumulateReadObjectPartialHandle>(
      std::move(cq), std::move(stream), timeout);
  return handle->Invoke();
}

future<AsyncAccumulateReadObjectResult> AsyncAccumulateReadObjectFull(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub,
    std::function<std::shared_ptr<grpc::ClientContext>()> context_factory,
    google::storage::v2::ReadObjectRequest request,
    google::cloud::internal::ImmutableOptions options) {
  auto handle = std::make_shared<AsyncAccumulateReadObjectFullHandle>(
      std::move(cq), std::move(stub), std::move(context_factory),
      std::move(request), std::move(options));
  return handle->Invoke();
}

StatusOr<storage_experimental::ReadPayload> ToResponse(
    AsyncAccumulateReadObjectResult accumulated) {
  if (!accumulated.status.ok()) return std::move(accumulated.status);
  absl::Cord contents;
  for (auto& r : accumulated.payload) {
    if (!r.has_checksummed_data()) continue;
    auto& data = *r.mutable_checksummed_data();
    if (data.has_crc32c() && Crc32c(GetContent(data)) != data.crc32c()) {
      return internal::DataLossError(
          "Mismatched CRC32C checksum in downloaded data", GCP_ERROR_INFO());
    }
    contents.Append(StealMutableContent(data));
  }

  storage_experimental::ReadPayload response(
      ReadPayloadImpl::Make(std::move(contents)));
  auto r = std::find_if(accumulated.payload.begin(), accumulated.payload.end(),
                        [](auto const& r) { return r.has_metadata(); });
  if (r != accumulated.payload.end()) {
    response.set_metadata(std::move(*r->mutable_metadata()));
  }

  storage::HeadersMap headers = std::move(accumulated.metadata.headers);
  headers.insert(std::make_move_iterator(accumulated.metadata.trailers.begin()),
                 std::make_move_iterator(accumulated.metadata.trailers.end()));
  response.set_headers(std::move(headers));

  if (!accumulated.payload.empty()) {
    response.set_offset(accumulated.payload.front().content_range().start());
  }

  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
