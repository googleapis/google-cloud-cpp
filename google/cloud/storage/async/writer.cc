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

#include "google/cloud/storage/async/writer.h"
#include "google/cloud/internal/make_status.h"
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

template <typename T>
future<StatusOr<T>> TokenError(google::cloud::internal::ErrorInfoBuilder eib) {
  return make_ready_future(
      StatusOr<T>(google::cloud::internal::InvalidArgumentError(
          "invalid token", std::move(eib))));
}

template <typename T>
future<StatusOr<T>> StreamError(google::cloud::internal::ErrorInfoBuilder eib) {
  return make_ready_future(StatusOr<T>(google::cloud::internal::CancelledError(
      "closed stream", std::move(eib))));
}

}  // namespace

AsyncWriter::AsyncWriter(std::unique_ptr<AsyncWriterConnection> impl)
    : impl_(std::move(impl)) {}

AsyncWriter::~AsyncWriter() = default;

std::string AsyncWriter::UploadId() const {
  if (impl_) return impl_->UploadId();
  return {};
}

std::variant<std::int64_t, google::storage::v2::Object>
AsyncWriter::PersistedState() const {
  if (impl_) return impl_->PersistedState();
  return -1;
}

future<StatusOr<AsyncToken>> AsyncWriter::Write(AsyncToken token,
                                                WritePayload payload) {
  if (!impl_) return StreamError<AsyncToken>(GCP_ERROR_INFO());
  auto t = storage_internal::MakeAsyncToken(impl_.get());
  if (token != t) return TokenError<AsyncToken>(GCP_ERROR_INFO());

  // `Write()` may invoke a callback synchronously that destroys `*this`, so
  // do not access member variables after the call.
  std::shared_ptr<AsyncWriterConnection> impl = impl_;
  return impl->Write(std::move(payload))
      .then([impl = std::move(impl),
             token = std::move(t)](future<Status> f) mutable {
        Status status = f.get();
        if (status.ok()) return make_status_or(std::move(token));
        return StatusOr<AsyncToken>(std::move(status));
      });
}

future<StatusOr<google::storage::v2::Object>> AsyncWriter::Finalize(
    AsyncToken token, WritePayload payload) {
  if (!impl_) return StreamError<google::storage::v2::Object>(GCP_ERROR_INFO());
  auto t = storage_internal::MakeAsyncToken(impl_.get());
  if (token != t) {
    return TokenError<google::storage::v2::Object>(GCP_ERROR_INFO());
  }

  // `Finalize()` may invoke a callback synchronously that destroys `*this`, so
  // do not access member variables after the call.
  std::shared_ptr<AsyncWriterConnection> impl = impl_;
  return impl->Finalize(std::move(payload))
      .then([impl = std::move(impl)](
                future<StatusOr<google::storage::v2::Object>> f) {
        return f.get();
      });
}

future<StatusOr<google::storage::v2::Object>> AsyncWriter::Finalize(
    AsyncToken token) {
  return Finalize(std::move(token), WritePayload{});
}

future<Status> AsyncWriter::Flush() {
  if (!impl_) {
    return make_ready_future(google::cloud::internal::CancelledError(
        "closed stream", GCP_ERROR_INFO()));
  }

  // `Flush()` may invoke a callback synchronously that destroys `*this`, so
  // do not access member variables after the call.
  std::shared_ptr<AsyncWriterConnection> impl = impl_;
  return impl->Flush(WritePayload{})
      .then([impl = std::move(impl)](future<Status> f) { return f.get(); });
}

future<Status> AsyncWriter::Close() {
  if (!impl_) {
    return make_ready_future(google::cloud::internal::CancelledError(
        "closed stream", GCP_ERROR_INFO()));
  }

  // Move `impl_` before `Close()`, as a synchronous callback may destroy
  // `*this` or re-enter this writer (which should then observe a closed
  // stream).
  std::shared_ptr<AsyncWriterConnection> impl = std::move(impl_);
  return impl->Close(WritePayload{})
      .then([impl = std::move(impl)](future<Status> f) { return f.get(); });
}

RpcMetadata AsyncWriter::GetRequestMetadata() const {
  return impl_->GetRequestMetadata();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
