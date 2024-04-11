// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/async/writer_connection_finalized.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

Status MakeError(google::cloud::internal::ErrorInfoBuilder eib) {
  return google::cloud::internal::FailedPreconditionError(
      "upload already finalized", std::move(eib));
}

}  // namespace

AsyncWriterConnectionFinalized::AsyncWriterConnectionFinalized(
    std::string upload_id, google::storage::v2::Object object)
    : upload_id_(std::move(upload_id)), object_(std::move(object)) {}

AsyncWriterConnectionFinalized::~AsyncWriterConnectionFinalized() = default;

void AsyncWriterConnectionFinalized::Cancel() {}

std::string AsyncWriterConnectionFinalized::UploadId() const {
  return upload_id_;
}

absl::variant<std::int64_t, google::storage::v2::Object>
AsyncWriterConnectionFinalized::PersistedState() const {
  return object_;
}

future<Status> AsyncWriterConnectionFinalized::Write(
    storage_experimental::WritePayload) {
  return make_ready_future(MakeError(GCP_ERROR_INFO()));
}

future<StatusOr<google::storage::v2::Object>>
AsyncWriterConnectionFinalized::Finalize(storage_experimental::WritePayload) {
  return make_ready_future(
      StatusOr<google::storage::v2::Object>(MakeError(GCP_ERROR_INFO())));
}

future<Status> AsyncWriterConnectionFinalized::Flush(
    storage_experimental::WritePayload) {
  return make_ready_future(MakeError(GCP_ERROR_INFO()));
}

future<StatusOr<std::int64_t>> AsyncWriterConnectionFinalized::Query() {
  return make_ready_future(StatusOr<std::int64_t>(MakeError(GCP_ERROR_INFO())));
}

RpcMetadata AsyncWriterConnectionFinalized::GetRequestMetadata() { return {}; }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
