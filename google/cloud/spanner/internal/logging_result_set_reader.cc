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

#include "google/cloud/spanner/internal/logging_result_set_reader.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::DebugString;

void LoggingResultSetReader::TryCancel() { impl_->TryCancel(); }

bool LoggingResultSetReader::Read(
    absl::optional<std::string> const& resume_token,
    UnownedPartialResultSet& result) {
  if (resume_token) {
    GCP_LOG(DEBUG) << __func__ << "() << resume_token=\""
                   << DebugString(*resume_token, tracing_options_) << "\"";
  } else {
    GCP_LOG(DEBUG) << __func__ << "() << (unresumable)";
  }
  bool success = impl_->Read(resume_token, result);
  if (!success) {
    GCP_LOG(DEBUG) << __func__ << "() >> (failed)";
  } else {
    GCP_LOG(DEBUG) << __func__ << "() >> resumption="
                   << (result.resumption ? "true" : "false");
  }
  return success;
}

Status LoggingResultSetReader::Finish() { return impl_->Finish(); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
