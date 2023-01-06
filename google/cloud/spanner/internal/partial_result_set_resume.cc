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

absl::optional<PartialResultSet> PartialResultSetResume::Read(
    absl::optional<std::string> const& resume_token) {
  bool resumption = false;
  do {
    absl::optional<PartialResultSet> result = child_->Read(resume_token);
    if (result) {
      // Let the caller know if we recreated the PartialResultSetReader using
      // the resume_token so that they might discard any previous results that
      // will be contained in the new stream.
      if (resumption) result->resumption = true;
      return result;
    }
    auto status = Finish();
    if (status.ok()) return {};
    if (!resume_token) {
      // Our caller has requested that we not try to resume the stream,
      // probably because they have already delivered previous results that
      // would otherwise be replayed.
      return {};
    }
    if (idempotency_ == google::cloud::Idempotency::kNonIdempotent ||
        !retry_policy_prototype_->OnFailure(status)) {
      return {};
    }
    std::this_thread::sleep_for(backoff_policy_prototype_->OnCompletion());
    resumption = true;
    last_status_.reset();
    child_ = factory_(*resume_token);
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
