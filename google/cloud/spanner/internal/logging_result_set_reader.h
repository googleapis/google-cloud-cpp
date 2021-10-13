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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_LOGGING_RESULT_SET_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_LOGGING_RESULT_SET_READER_H

#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/spanner/version.h"
#include "absl/types/optional.h"
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class LoggingResultSetReader : public PartialResultSetReader {
 public:
  LoggingResultSetReader(std::unique_ptr<PartialResultSetReader> impl,
                         TracingOptions tracing_options)
      : impl_(std::move(impl)), tracing_options_(std::move(tracing_options)) {}
  ~LoggingResultSetReader() override = default;

  void TryCancel() override;
  absl::optional<google::spanner::v1::PartialResultSet> Read() override;
  Status Finish() override;

 private:
  std::unique_ptr<PartialResultSetReader> impl_;
  TracingOptions tracing_options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_LOGGING_RESULT_SET_READER_H
