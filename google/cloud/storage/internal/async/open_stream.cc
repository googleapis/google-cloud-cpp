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
  if (cancel_.exchange(true)) return;
  rpc_->Cancel();
  MaybeFinish();
}

future<bool> OpenStream::Start() {
  pending_start_.store(true);
  if (cancel_.load()) {
    OnStart();
    return make_ready_future(false);
  }
  return rpc_->Start().then([s = shared_from_this()](future<bool> f) mutable {
    bool const ok = f.get();
    s->OnStart();
    return !s->cancel_.load() && ok;
  });
}

future<bool> OpenStream::Write(
    google::storage::v2::BidiReadObjectRequest const& request) {
  pending_write_.store(true);
  if (cancel_.load()) {
    OnWrite();
    return make_ready_future(false);
  }
  return rpc_->Write(request, grpc::WriteOptions{})
      .then([s = shared_from_this()](future<bool> f) mutable {
        bool const ok = f.get();
        s->OnWrite();
        return !s->cancel_.load() && ok;
      });
}

future<OpenStream::ReadType> OpenStream::Read() {
  pending_read_.store(true);
  if (cancel_.load()) {
    OnRead();
    return make_ready_future(ReadType(std::nullopt));
  }
  return rpc_->Read().then(
      [s = shared_from_this()](future<ReadType> f) mutable {
        ReadType result = f.get();
        s->OnRead();
        if (s->cancel_.load()) return ReadType(std::nullopt);
        return result;
      });
}

future<Status> OpenStream::Finish() {
  if (finish_issued_.exchange(true)) return {};
  return rpc_->Finish().then(
      [s = shared_from_this()](future<Status> f) mutable {
        Status result = f.get();
        s.reset();
        return result;
      });
}

void OpenStream::OnStart() {
  pending_start_.store(false);
  MaybeFinish();
}

void OpenStream::OnWrite() {
  pending_write_.store(false);
  MaybeFinish();
}

void OpenStream::OnRead() {
  pending_read_.store(false);
  MaybeFinish();
}

void OpenStream::MaybeFinish() {
  if (!cancel_.load()) return;
  // Sequential consistency (`memory_order_seq_cst`) is required here: each
  // operation stores `pending_*_ = true` before loading `cancel_`, while
  // `Cancel()` stores `cancel_ = true` before `MaybeFinish()` loads
  // `pending_*_` (Dekker's store-then-load pattern).
  if (pending_start_.load() || pending_read_.load() || pending_write_.load()) {
    return;
  }
  Finish();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
