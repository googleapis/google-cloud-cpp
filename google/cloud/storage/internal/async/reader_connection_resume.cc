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

#include "google/cloud/storage/internal/async/reader_connection_resume.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::storage_experimental::AsyncReaderConnection;
using ::google::cloud::storage_experimental::ResumePolicy;
using ReadResponse =
    google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;

void AsyncReaderConnectionResume::Cancel() {
  if (auto impl = CurrentImpl()) return impl->Cancel();
}

future<ReadResponse> AsyncReaderConnectionResume::Read() {
  return Read(std::unique_lock<std::mutex>(mu_));
}

RpcMetadata AsyncReaderConnectionResume::GetRequestMetadata() {
  if (auto impl = CurrentImpl()) return impl->GetRequestMetadata();
  return {};
}

future<ReadResponse> AsyncReaderConnectionResume::Read(
    std::unique_lock<std::mutex> lk) {
  auto impl = CurrentImpl(lk);
  lk.unlock();
  if (!impl) return Reconnect();
  // Capturing `this` is safe. This class is used as part of the
  // `AsyncReaderConnection` stack, managed by a `AsyncReader`. We already warn
  // users to extend the lifetime of an `AsyncReaderConnection` until any
  // pending `Read()` calls complete successfully. And the `AsyncReader` class
  // ensures this is the case.
  return impl->Read().then([this](auto f) { return OnRead(f.get()); });
}

future<ReadResponse> AsyncReaderConnectionResume::OnRead(ReadResponse r) {
  if (absl::holds_alternative<storage_experimental::ReadPayload>(r)) {
    auto const& response = absl::get<storage_experimental::ReadPayload>(r);
    received_bytes_ += response.size();
    if (response.metadata().has_value() && !generation_.has_value()) {
      generation_ = storage::Generation(response.metadata()->generation());
    }
    return make_ready_future(std::move(r));
  }
  auto const& status = absl::get<Status>(r);
  if (status.ok() || resume_policy_->OnFinish(status) == ResumePolicy::kStop) {
    return make_ready_future(std::move(r));
  }
  return Reconnect();
}

future<ReadResponse> AsyncReaderConnectionResume::Reconnect() {
  // Capturing `this` is safe here. See the comments in the implementation of
  // `Read()` for details.
  return reader_factory_(generation_, received_bytes_).then([this](auto f) {
    return OnResume(f.get());
  });
}

future<ReadResponse> AsyncReaderConnectionResume::OnResume(
    StatusOr<std::unique_ptr<AsyncReaderConnection>> connection) {
  if (!connection) {
    return make_ready_future(ReadResponse(std::move(connection).status()));
  }
  received_bytes_ = 0;
  resume_policy_->OnStartSuccess();
  std::unique_lock<std::mutex> lk(mu_);
  impl_ = *std::move(connection);
  return Read(std::move(lk));
}

std::shared_ptr<storage_experimental::AsyncReaderConnection>
AsyncReaderConnectionResume::CurrentImpl(std::unique_lock<std::mutex> const&) {
  return impl_;
}

std::shared_ptr<storage_experimental::AsyncReaderConnection>
AsyncReaderConnectionResume::CurrentImpl() {
  return CurrentImpl(std::unique_lock<std::mutex>(mu_));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
