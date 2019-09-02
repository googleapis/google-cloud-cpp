// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/log.h"
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
RetryObjectReadSource::RetryObjectReadSource(
    std::shared_ptr<RetryClient> client, ReadObjectRangeRequest request,
    std::unique_ptr<ObjectReadSource> child,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy)
    : client_(std::move(client)),
      request_(std::move(request)),
      child_(std::move(child)),
      current_offset_(request_.StartingByte()),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_prototype_(std::move(backoff_policy)) {}

StatusOr<ReadSourceResult> RetryObjectReadSource::Read(char* buf,
                                                       std::size_t n) {
  GCP_LOG(INFO) << __func__ << "() current_offset=" << current_offset_;
  if (!child_) {
    return Status(StatusCode::kFailedPrecondition, "Stream is not open");
  }
  auto result = child_->Read(buf, n);
  // Whenever we successfully get a piece of data, we should reset the backoff
  // policy to its initial state. It is easier to keep it fresh in the object
  // and clone on every Read(). By the time we return from this function we will
  // have either successfully read a piece of data or we will have failed for
  // good.
  auto backoff_policy = backoff_policy_prototype_->clone();
  for (; !result && retry_policy_->OnFailure(result.status());
       std::this_thread::sleep_for(backoff_policy->OnCompletion()),
       result = child_->Read(buf, n)) {
    // A Read() request failed, most likely that means the connection failed or
    // stalled. The current child might no longer be usable, so we will try to
    // create a new one and replace it. Should that fail, the retry policy would
    // already be exhausted, so we should fail this operation too.
    child_.reset();
    request_.set_option(ReadFromOffset(current_offset_));
    if (generation_) {
      request_.set_option(Generation(*generation_));
    }
    auto new_child = client_->ReadObjectNotWrapped(request_, *retry_policy_,
                                                   *backoff_policy);
    if (!new_child) {
      // We've exhausted the retry policy while trying to create the child, so
      // return right away.
      return new_child.status();
    }
    child_ = std::move(*new_child);
  }
  if (!result) {
    // We've tried retrying but failed.
    auto const& status = result.status();
    std::stringstream os;
    if (internal::StatusTraits::IsPermanentFailure(status)) {
      os << "Permanent error in Read(): " << status;
    } else {
      os << "Retry policy exhausted in Read(): " << status;
    }
    return Status(status.code(), os.str());
  }
  auto g = result->response.headers.find("x-goog-generation");
  if (g != result->response.headers.end()) {
    generation_ = std::stoll(g->second);
  }
  current_offset_ += result->bytes_received;
  return result;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
