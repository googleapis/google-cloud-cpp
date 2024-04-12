// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_REWRITER_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_REWRITER_CONNECTION_IMPL_H

#include "google/cloud/storage/async/rewriter_connection.h"
#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class RewriterConnectionImpl
    : public storage_experimental::AsyncRewriterConnection,
      public std::enable_shared_from_this<RewriterConnectionImpl> {
 public:
  RewriterConnectionImpl(CompletionQueue cq, std::shared_ptr<StorageStub> stub,
                         google::cloud::internal::ImmutableOptions current,
                         google::storage::v2::RewriteObjectRequest request);
  ~RewriterConnectionImpl() override = default;

  future<StatusOr<google::storage::v2::RewriteResponse>> Iterate() override;

 private:
  std::weak_ptr<RewriterConnectionImpl> WeakFromThis() {
    return shared_from_this();
  }

  StatusOr<google::storage::v2::RewriteResponse> OnRewrite(
      StatusOr<google::storage::v2::RewriteResponse> response);

  CompletionQueue cq_;
  std::shared_ptr<StorageStub> stub_;
  google::cloud::internal::ImmutableOptions current_;
  google::storage::v2::RewriteObjectRequest request_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_REWRITER_CONNECTION_IMPL_H
