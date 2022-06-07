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
#include "google/cloud/internal/log_wrapper.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::DebugString;

void LoggingResultSetReader::TryCancel() {
  GCP_LOG(DEBUG) << __func__ << "() << (void)";
  impl_->TryCancel();
  GCP_LOG(DEBUG) << __func__ << "() >> (void)";
}

absl::optional<PartialResultSet> LoggingResultSetReader::Read() {
  GCP_LOG(DEBUG) << __func__ << "() << (void)";
  auto result = impl_->Read();
  if (!result) {
    GCP_LOG(DEBUG) << __func__ << "() >> (optional-with-no-value)";
  } else {
    GCP_LOG(DEBUG) << __func__ << "() >> "
                   << (result->resumption ? "resumption " : "")
                   << DebugString(result->result, tracing_options_);
  }
  return result;
}

Status LoggingResultSetReader::Finish() {
  GCP_LOG(DEBUG) << __func__ << "() << (void)";
  auto status = impl_->Finish();
  GCP_LOG(DEBUG) << __func__ << "() >> "
                 << DebugString(status, tracing_options_);
  return status;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
