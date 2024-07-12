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
  cancel_.store(true);
  rpc_->Cancel();
  MaybeFinish();
}

future<bool> OpenStream::Start() {
  if (cancel_) return make_ready_future(false);
  return rpc_->Start();
}

future<bool> OpenStream::Write(
    google::storage::v2::BidiReadObjectRequest const& request) {
  if (cancel_) return make_ready_future(false);
  pending_write_.store(true);
  return rpc_->Write(request, grpc::WriteOptions{})
      .then([s = shared_from_this()](auto f) mutable {
        auto self = std::move(s);
        self->OnWrite();
        return f.get();
      });
}

future<OpenStream::ReadType> OpenStream::Read() {
  if (cancel_) return make_ready_future(ReadType(absl::nullopt));
  pending_read_.store(true);
  return rpc_->Read().then([s = shared_from_this()](auto f) mutable {
    auto self = std::move(s);
    self->OnRead();
    return f.get();
  });
}

future<Status> OpenStream::Finish() {
  finish_issued_.store(true);
  return rpc_->Finish().then([s = shared_from_this()](auto f) mutable {
    auto result = f.get();
    s.reset();
    return result;
  });
}

void OpenStream::OnWrite() {
  pending_write_.store(false);
  return MaybeFinish();
}

void OpenStream::OnRead() {
  pending_read_.store(false);
  return MaybeFinish();
}

void OpenStream::MaybeFinish() {
  if (!cancel_) return;
  // Once `cancel_` is true these can only become false.
  if (pending_read_ || pending_write_ || finish_issued_) return;
  Finish();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
