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
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"

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
    resume_policy_->OnStartSuccess();
    auto response = absl::get<storage_experimental::ReadPayload>(std::move(r));
    hash_validator_->ProcessHashValues(
        ReadPayloadImpl::GetObjectHashes(response).value_or(
            storage::internal::HashValues{}));
    received_bytes_ += response.size();
    if (response.metadata().has_value() && !generation_.has_value()) {
      generation_ = storage::Generation(response.metadata()->generation());
    }
    return make_ready_future(ReadResponse(std::move(response)));
  }
  auto const& status = absl::get<Status>(r);
  if (status.ok()) {
    // The download finished. Validate the hash results, if any.
    auto result = std::move(*hash_validator_).Finish(hash_function_->Finish());
    if (!result.is_mismatch) return make_ready_future(std::move(r));
    return make_ready_future(ReadResponse(internal::InvalidArgumentError(
        absl::StrCat("mismatched checksums detected at the end of the "
                     "download, received={",
                     FormatReceivedHashes(result), "}, computed={",
                     FormatComputedHashes(result), "}"),
        GCP_ERROR_INFO())));
  }
  if (resume_policy_->OnFinish(status) == ResumePolicy::kStop) {
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
