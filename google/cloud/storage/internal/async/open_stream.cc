// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/open_stream.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

OpenStream::OpenStream(std::unique_ptr<StreamingRpc> rpc)
    : rpc_(std::move(rpc)) {}

void OpenStream::Cancel() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (cancel_) return;
    cancel_ = true;
    ++pending_ops_;
    if (!finish_issued_) {
      finish_issued_ = true;
      finish_deferred_ = true;
      finish_promise_ = promise<Status>{};
      finish_ = finish_promise_.get_future();
    }
  }
  rpc_->Cancel();
  EndOp();
}

future<bool> OpenStream::Start() {
  if (!BeginOp()) return make_ready_future(false);
  return rpc_->Start().then([s = shared_from_this()](future<bool> f) mutable {
    std::shared_ptr<OpenStream> self = std::move(s);
    bool const cancelled = self->EndOp();
    bool const ok = f.get();
    return !cancelled && ok;
  });
}

future<bool> OpenStream::Write(
    google::storage::v2::BidiReadObjectRequest const& request) {
  if (!BeginOp()) return make_ready_future(false);
  return rpc_->Write(request, grpc::WriteOptions{})
      .then([s = shared_from_this()](future<bool> f) mutable {
        std::shared_ptr<OpenStream> self = std::move(s);
        bool const cancelled = self->EndOp();
        bool const ok = f.get();
        return !cancelled && ok;
      });
}

future<OpenStream::ReadType> OpenStream::Read() {
  if (!BeginOp()) return make_ready_future(ReadType(std::nullopt));
  return rpc_->Read().then(
      [s = shared_from_this()](future<ReadType> f) mutable {
        std::shared_ptr<OpenStream> self = std::move(s);
        bool const cancelled = self->EndOp();
        ReadType result = f.get();
        if (cancelled) return ReadType(std::nullopt);
        return result;
      });
}

future<Status> OpenStream::Finish() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (finish_issued_) return std::move(finish_);
    finish_issued_ = true;
  }
  return rpc_->Finish().then(
      [s = shared_from_this()](future<Status> f) mutable {
        s.reset();
        return f.get();
      });
}

bool OpenStream::BeginOp() {
  std::lock_guard<std::mutex> lk(mu_);
  if (cancel_ || finish_issued_) return false;
  ++pending_ops_;
  return true;
}

bool OpenStream::EndOp() {
  promise<Status> deferred_finish{null_promise_t{}};
  bool issue_finish = false;
  bool cancelled = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    --pending_ops_;
    cancelled = cancel_;
    if (finish_deferred_ && pending_ops_ == 0) {
      finish_deferred_ = false;
      deferred_finish = std::move(finish_promise_);
      issue_finish = true;
    }
  }
  if (issue_finish) {
    rpc_->Finish().then(
        [s = shared_from_this(),
         p = std::move(deferred_finish)](future<Status> f) mutable {
          promise<Status> out = std::move(p);
          s.reset();
          out.set_value(f.get());
        });
  }
  return cancelled;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
