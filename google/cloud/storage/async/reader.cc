// Copyright 2023 Google LLC
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

#include "google/cloud/storage/async/reader.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/internal/make_status.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

StatusOr<std::pair<ReadPayload, AsyncToken>> InvalidTokenError(
    internal::ErrorInfoBuilder eib) {
  return internal::InvalidArgumentError("invalid token", std::move(eib));
}

StatusOr<std::pair<ReadPayload, AsyncToken>> ClosedStreamError(
    internal::ErrorInfoBuilder eib) {
  return internal::CancelledError("closed stream", std::move(eib));
}

class Discard : public std::enable_shared_from_this<Discard> {
 public:
  explicit Discard(std::unique_ptr<AsyncReaderConnection> impl)
      : impl_(std::move(impl)) {}

  void Loop() {
    auto self = shared_from_this();
    self->impl_->Read().then([self](auto f) mutable {
      auto s = std::move(self);
      auto response = f.get();
      if (absl::holds_alternative<Status>(response)) return;
      s->Loop();
    });
  }

 private:
  std::unique_ptr<AsyncReaderConnection> impl_;
};

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
AsyncReader::~AsyncReader() {
  if (!impl_ || finished_) return;
  impl_->Cancel();
  auto discard = std::make_shared<Discard>(std::move(impl_));
  discard->Loop();
}

future<StatusOr<std::pair<ReadPayload, AsyncToken>>> AsyncReader::Read(
    AsyncToken token) {
  if (!impl_) return make_ready_future(ClosedStreamError(GCP_ERROR_INFO()));
  auto t = storage_internal::MakeAsyncToken(impl_.get());
  if (token != t) return make_ready_future(InvalidTokenError(GCP_ERROR_INFO()));

  struct Visitor {
    AsyncToken token;
    StatusOr<std::pair<ReadPayload, AsyncToken>> operator()(Status s) {
      if (s.ok()) return std::make_pair(ReadPayload{}, AsyncToken{});
      return s;
    }

    StatusOr<std::pair<ReadPayload, AsyncToken>> operator()(ReadPayload p) {
      return std::make_pair(std::move(p), std::move(token));
    }
  };
  return impl_->Read().then([this, v = Visitor{std::move(t)}](auto f) mutable {
    auto response = f.get();
    if (absl::holds_alternative<Status>(response)) finished_ = true;
    return absl::visit(std::move(v), std::move(response));
  });
}

RpcMetadata AsyncReader::GetRequestMetadata() {
  if (!finished_) return {};
  return impl_->GetRequestMetadata();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
