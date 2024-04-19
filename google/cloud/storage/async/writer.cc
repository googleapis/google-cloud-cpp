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

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

template <typename T>
future<StatusOr<T>> TokenError(internal::ErrorInfoBuilder eib) {
  return make_ready_future(StatusOr<T>(
      internal::InvalidArgumentError("invalid token", std::move(eib))));
}

template <typename T>
future<StatusOr<T>> StreamError(internal::ErrorInfoBuilder eib) {
  return make_ready_future(
      StatusOr<T>(internal::CancelledError("closed stream", std::move(eib))));
}

}  // namespace

AsyncWriter::AsyncWriter(std::unique_ptr<AsyncWriterConnection> impl)
    : impl_(std::move(impl)) {}

AsyncWriter::~AsyncWriter() = default;

std::string AsyncWriter::UploadId() const {
  if (impl_) return impl_->UploadId();
  return {};
}

absl::variant<std::int64_t, google::storage::v2::Object>
AsyncWriter::PersistedState() const {
  if (impl_) return impl_->PersistedState();
  return -1;
}

future<StatusOr<AsyncToken>> AsyncWriter::Write(AsyncToken token,
                                                WritePayload payload) {
  if (!impl_) return StreamError<AsyncToken>(GCP_ERROR_INFO());
  auto t = storage_internal::MakeAsyncToken(impl_.get());
  if (token != t) return TokenError<AsyncToken>(GCP_ERROR_INFO());

  return impl_->Write(std::move(payload))
      .then([impl = impl_, token = std::move(t)](auto f) mutable {
        auto status = f.get();
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

  return impl_->Finalize(std::move(payload)).then([impl = impl_](auto f) {
    return f.get();
  });
}

future<StatusOr<google::storage::v2::Object>> AsyncWriter::Finalize(
    AsyncToken token) {
  return Finalize(std::move(token), WritePayload{});
}

RpcMetadata AsyncWriter::GetRequestMetadata() const {
  return impl_->GetRequestMetadata();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
