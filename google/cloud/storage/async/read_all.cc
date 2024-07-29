// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/read_all.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class ReadAllImpl : public std::enable_shared_from_this<ReadAllImpl> {
 public:
  explicit ReadAllImpl(AsyncReader reader) : reader_(std::move(reader)) {}

  future<StatusOr<ReadPayload>> Start(AsyncToken token) {
    auto f = promise_.get_future();
    DoRead(std::move(token));
    return f;
  }

 private:
  std::weak_ptr<ReadAllImpl> WeakFromThis() { return shared_from_this(); }

  void DoRead(AsyncToken token) {
    if (!token.valid()) return promise_.set_value(std::move(accumulated_));
    reader_.Read(std::move(token)).then([w = WeakFromThis()](auto f) {
      if (auto self = w.lock()) self->OnRead(f.get());
    });
  }

  void OnRead(StatusOr<std::pair<ReadPayload, AsyncToken>> response) {
    if (!response) return promise_.set_value(std::move(response).status());
    ReadPayload payload;
    AsyncToken token;
    std::tie(payload, token) = *std::move(response);
    storage_internal::ReadPayloadImpl::Accumulate(accumulated_,
                                                  std::move(payload));
    DoRead(std::move(token));
  }

  AsyncReader reader_;
  AsyncToken token_;
  promise<StatusOr<ReadPayload>> promise_;
  ReadPayload accumulated_;
};

}  // namespace

future<StatusOr<ReadPayload>> ReadAll(AsyncReader reader, AsyncToken token) {
  auto impl = std::make_shared<ReadAllImpl>(std::move(reader));
  return impl->Start(std::move(token)).then([impl](auto f) mutable {
    impl.reset();
    return f.get();
  });
}

future<StatusOr<ReadPayload>> ReadAll(
    StatusOr<std::pair<AsyncReader, AsyncToken>> read) {
  if (!read) {
    return make_ready_future(StatusOr<ReadPayload>(std::move(read).status()));
  }
  AsyncReader reader;
  AsyncToken token;
  std::tie(reader, token) = *std::move(read);
  return ReadAll(std::move(reader), std::move(token));
}

future<StatusOr<ReadPayload>> ReadAll(
    future<StatusOr<std::pair<AsyncReader, AsyncToken>>> pending_read) {
  return pending_read.then([](auto f) { return ReadAll(f.get()); });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
