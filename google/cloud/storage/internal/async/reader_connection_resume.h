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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_RESUME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_RESUME_H

#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/internal/async/reader_connection_factory.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include "google/storage/v2/storage.pb.h"
#include <cstdint>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class AsyncReaderConnectionResume
    : public storage_experimental::AsyncReaderConnection {
 public:
  explicit AsyncReaderConnectionResume(
      std::unique_ptr<storage_experimental::ResumePolicy> resume_policy,
      std::shared_ptr<storage::internal::HashFunction> hash,
      std::unique_ptr<storage::internal::HashValidator> validator,
      AsyncReaderConnectionFactory reader_factory)
      : resume_policy_(std::move(resume_policy)),
        hash_function_(std::move(hash)),
        hash_validator_(std::move(validator)),
        reader_factory_(std::move(reader_factory)) {}

  void Cancel() override;

  future<ReadResponse> Read() override;
  RpcMetadata GetRequestMetadata() override;

 private:
  future<ReadResponse> Read(std::unique_lock<std::mutex> lk);
  future<ReadResponse> OnRead(ReadResponse r);
  future<ReadResponse> Reconnect();
  future<ReadResponse> OnResume(
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>
          connection);
  std::shared_ptr<storage_experimental::AsyncReaderConnection> CurrentImpl(
      std::unique_lock<std::mutex> const&);
  std::shared_ptr<storage_experimental::AsyncReaderConnection> CurrentImpl();

  std::unique_ptr<storage_experimental::ResumePolicy> resume_policy_;
  std::shared_ptr<storage::internal::HashFunction> hash_function_;
  std::unique_ptr<storage::internal::HashValidator> hash_validator_;
  AsyncReaderConnectionFactory reader_factory_;
  storage::Generation generation_;
  std::int64_t received_bytes_ = 0;
  std::mutex mu_;
  std::shared_ptr<storage_experimental::AsyncReaderConnection> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_RESUME_H
