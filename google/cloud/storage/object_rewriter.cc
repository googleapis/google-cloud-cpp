// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/object_rewriter.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/internal/throw_status_delegate.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
ObjectRewriter::ObjectRewriter(std::shared_ptr<internal::RawClient> client,
                               internal::RewriteObjectRequest request)
    : client_(std::move(client)),
      request_(std::move(request)),
      progress_{0, 0, false} {}

RewriteProgress ObjectRewriter::Iterate() {
  internal::RewriteObjectResponse response =
      client_->RewriteObject(request_).value();
  progress_ = RewriteProgress{response.total_bytes_rewritten,
                              response.object_size, response.done};
  if (response.done) {
    result_ = std::move(response.resource);
  }
  request_.set_rewrite_token(std::move(response.rewrite_token));
  return progress_;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
