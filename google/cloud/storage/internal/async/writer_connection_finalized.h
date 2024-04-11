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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITER_CONNECTION_FINALIZED_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITER_CONNECTION_FINALIZED_H

#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Implement the `AsyncWriterConnection` interface for resumed, but finalized,
 * uploads.
 *
 * Applications may resume an upload that was already finalized. For example,
 * the application may resume all pending uploads when it starts, using some
 * kind of database to keep the pending uploads. Such an application may:
 *
 * - Finalize an upload.
 * - Crash or be terminated before having an opportunity to update its database.
 * - Try to resume the upload.
 *
 * At this point the application would discover the upload is finalized.
 *
 * In this case we want to return an implementation of `AsyncWriterConnection`
 * that contains the result of the finalized upload, but does not require an
 * underlying streaming RPC. No such RPC is needed or can successfully upload
 * additional data.
 */
class AsyncWriterConnectionFinalized
    : public storage_experimental::AsyncWriterConnection {
 public:
  explicit AsyncWriterConnectionFinalized(std::string upload_id,
                                          google::storage::v2::Object object);
  ~AsyncWriterConnectionFinalized() override;

  void Cancel() override;

  std::string UploadId() const override;
  absl::variant<std::int64_t, google::storage::v2::Object> PersistedState()
      const override;

  future<Status> Write(storage_experimental::WritePayload payload) override;
  future<StatusOr<google::storage::v2::Object>> Finalize(
      storage_experimental::WritePayload) override;
  future<Status> Flush(storage_experimental::WritePayload payload) override;
  future<StatusOr<std::int64_t>> Query() override;
  RpcMetadata GetRequestMetadata() override;

 private:
  std::string upload_id_;
  google::storage::v2::Object object_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITER_CONNECTION_FINALIZED_H
