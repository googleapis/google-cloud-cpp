// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/partial_result_set_resume.h"
#include <thread>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void PartialResultSetResume::TryCancel() { child_->TryCancel(); }

absl::optional<PartialResultSet> PartialResultSetResume::Read() {
  bool resumption = false;
  do {
    absl::optional<PartialResultSet> result = child_->Read();
    if (result) {
      // If the resume_token is empty then this PartialResultSet does not
      // contain enough data for PartialResultSetSource to be able to yield
      // a new row, so we should leave last_resume_token_ as is---ready to
      // re-request this undelivered chunk should a following Read() fail.
      if (!result->result.resume_token().empty()) {
        last_resume_token_ = result->result.resume_token();
      }
      // Let the caller know if we recreated the PartialResultSetReader using
      // last_resume_token_ so that they might discard any pending row-assembly
      // state as that data will also be in this new result.
      result->resumption = resumption;
      return result;
    }
    auto status = Finish();
    if (status.ok()) return {};
    if (idempotency_ == google::cloud::Idempotency::kNonIdempotent ||
        !retry_policy_prototype_->OnFailure(status)) {
      return {};
    }
    std::this_thread::sleep_for(backoff_policy_prototype_->OnCompletion());
    resumption = true;
    last_status_.reset();
    child_ = factory_(last_resume_token_);
  } while (!retry_policy_prototype_->IsExhausted());
  return {};
}

Status PartialResultSetResume::Finish() {
  // Finish() can be called only once, so cache the last result.
  if (last_status_.has_value()) {
    return *last_status_;
  }
  last_status_ = child_->Finish();
  return *last_status_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
