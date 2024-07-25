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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_IMPL_H

#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <cstdint>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class AsyncReaderConnectionImpl
    : public storage_experimental::AsyncReaderConnection {
 public:
  using ProtoPayload = google::storage::v2::ReadObjectResponse;
  using StreamingRpc =
      ::google::cloud::internal::AsyncStreamingReadRpc<ProtoPayload>;

  explicit AsyncReaderConnectionImpl(
      google::cloud::internal::ImmutableOptions options,
      std::unique_ptr<StreamingRpc> impl,
      std::shared_ptr<storage::internal::HashFunction> hash_function)
      : options_(std::move(options)),
        impl_(std::move(impl)),
        hash_function_(std::move(hash_function)) {}

  void Cancel() override { return impl_->Cancel(); }

  future<ReadResponse> Read() override;
  RpcMetadata GetRequestMetadata() override;

 private:
  future<ReadResponse> OnRead(absl::optional<ProtoPayload> r);
  future<ReadResponse> HandleHashError(Status status);
  future<ReadResponse> DoFinish();

  google::cloud::internal::ImmutableOptions options_;
  std::shared_ptr<storage::internal::HashFunction> hash_;
  std::unique_ptr<StreamingRpc> impl_;
  std::shared_ptr<storage::internal::HashFunction> hash_function_;
  std::int64_t offset_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_IMPL_H
