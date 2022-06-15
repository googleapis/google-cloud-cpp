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

#include "google/cloud/storage/internal/async_accumulate_read_object.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

future<AsyncAccumulateReadObject::Result> AsyncAccumulateReadObject::Start(
    CompletionQueue cq, std::unique_ptr<Stream> stream,
    std::chrono::milliseconds timeout) {
  // Private constructor, std::make_shared<> does not work here.
  auto handle = std::shared_ptr<AsyncAccumulateReadObject>(
      new AsyncAccumulateReadObject(std::move(cq), std::move(stream), timeout));

  handle->DoStart();

  return handle->promise_.get_future();
}

AsyncAccumulateReadObject::AsyncAccumulateReadObject(
    CompletionQueue cq, std::unique_ptr<Stream> stream,
    std::chrono::milliseconds timeout)
    : cq_(std::move(cq)), stream_(std::move(stream)), timeout_(timeout) {}

void AsyncAccumulateReadObject::DoStart() {
  struct ByMove {
    std::shared_ptr<AsyncAccumulateReadObject> self;
    future<bool> tm;
    void operator()(future<bool> f) { self->OnStart(std::move(tm), f.get()); }
  };
  auto tm = MakeTimeout();
  stream_->Start().then(ByMove{shared_from_this(), std::move(tm)});
}

void AsyncAccumulateReadObject::OnStart(future<bool> tm, bool ok) {
  tm.cancel();
  if (tm.get()) return OnTimeout("Start()");
  if (!ok) return Finish();
  Read();
}

void AsyncAccumulateReadObject::Read() {
  struct ByMove {
    std::shared_ptr<AsyncAccumulateReadObject> self;
    future<bool> tm;
    void operator()(future<absl::optional<Response>> f) {
      self->OnRead(std::move(tm), f.get());
    }
  };
  auto tm = MakeTimeout();
  stream_->Read().then(ByMove{shared_from_this(), std::move(tm)});
}

void AsyncAccumulateReadObject::OnRead(future<bool> tm,
                                       absl::optional<Response> response) {
  tm.cancel();
  if (tm.get()) return OnTimeout("Read()");
  if (!response.has_value()) return Finish();
  accumulator_.push_back(*std::move(response));
  Read();
}

void AsyncAccumulateReadObject::Finish() {
  struct ByMove {
    std::shared_ptr<AsyncAccumulateReadObject> self;
    future<bool> tm;
    void operator()(future<Status> f) {
      self->OnFinish(std::move(tm), f.get());
    }
  };
  auto tm = MakeTimeout();
  stream_->Finish().then(ByMove{shared_from_this(), std::move(tm)});
}

void AsyncAccumulateReadObject::OnFinish(future<bool> tm, Status status) {
  tm.cancel();
  promise_.set_value(Result{std::move(status), std::move(accumulator_),
                            stream_->GetRequestMetadata()});
}

future<bool> AsyncAccumulateReadObject::MakeTimeout() {
  auto self = shared_from_this();
  return cq_.MakeRelativeTimer(timeout_).then(
      [self](future<StatusOr<std::chrono::system_clock::time_point>> f) {
        if (!f.get().ok()) return false;
        self->stream_->Cancel();
        return true;
      });
}

void AsyncAccumulateReadObject::OnTimeout(char const* where) {
  auto finish = stream_->Finish();
  finish.then(WaitForFinish{std::move(stream_)});
  promise_.set_value(Result{Status(StatusCode::kDeadlineExceeded,
                                   std::string{"Timeout waiting for "} + where),
                            std::move(accumulator_),
                            {}});
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
