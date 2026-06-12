// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_RESULT_SET_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_RESULT_SET_READER_H

#include "google/cloud/bigtable/internal/partial_result_set_reader.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/tracing_options.h"
#include "absl/types/optional.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A `PartialResultSetReader` decorator that logs the `Read()` `resume_token`
 * parameter and the `PartialResultSet::resumption` return value. This is an
 * extension to the standard `BigtableLogging` request/response logging.
 */
class LoggingResultSetReader : public PartialResultSetReader {
 public:
  LoggingResultSetReader(std::unique_ptr<PartialResultSetReader> impl,
                         TracingOptions tracing_options)
      : impl_(std::move(impl)), tracing_options_(std::move(tracing_options)) {}
  ~LoggingResultSetReader() override = default;

  void TryCancel() override;
  bool Read(absl::optional<std::string> const& resume_token,
            UnownedPartialResultSet& result) override;
  grpc::ClientContext const& context() const override;
  Status Finish() override;

 private:
  std::unique_ptr<PartialResultSetReader> impl_;
  TracingOptions tracing_options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_RESULT_SET_READER_H
