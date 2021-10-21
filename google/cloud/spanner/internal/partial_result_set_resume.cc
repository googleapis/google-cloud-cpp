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

#include "google/cloud/spanner/internal/partial_result_set_resume.h"
#include <thread>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

void PartialResultSetResume::TryCancel() { child_->TryCancel(); }

absl::optional<google::spanner::v1::PartialResultSet>
PartialResultSetResume::Read() {
  do {
    absl::optional<google::spanner::v1::PartialResultSet> result =
        child_->Read();
    if (result) {
      last_resume_token_ = result->resume_token();
      return result;
    }
    auto status = Finish();
    if (status.ok()) return {};
    if (idempotency_ == google::cloud::internal::Idempotency::kNonIdempotent ||
        !retry_policy_prototype_->OnFailure(status)) {
      return {};
    }
    std::this_thread::sleep_for(backoff_policy_prototype_->OnCompletion());
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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
