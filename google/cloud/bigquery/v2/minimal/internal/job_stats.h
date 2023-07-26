// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_STATS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_STATS_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_query_stats.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

// Describes whether this job was a statement or expression.
//
// For more details, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#EvaluationKind
struct EvaluationKind {
  static EvaluationKind UnSpecified();
  static EvaluationKind Statement();
  static EvaluationKind Expression();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvaluationKind, value);
inline bool operator==(EvaluationKind const& lhs, EvaluationKind const& rhs) {
  return lhs.value == rhs.value;
}

// Represents the location of the statement/expression being evaluated.
//
// For more details on how frames are evaluated, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#ScriptStackFrame
struct ScriptStackFrame {
  std::int32_t start_line;
  std::int32_t start_column;
  std::int32_t end_line;
  std::int32_t end_column;
  std::string procedure_id;
  std::string text;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ScriptStackFrame const& s);
void from_json(nlohmann::json const& j, ScriptStackFrame& s);
bool operator==(ScriptStackFrame const& lhs, ScriptStackFrame const& rhs);

// For a child job of a Script, describes information about the context
// of the job within the script.
//
// For more details, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#scriptstatistics
struct ScriptStatistics {
  EvaluationKind evaluation_kind;
  std::vector<ScriptStackFrame> stack_frames;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ScriptStatistics const& s);
void from_json(nlohmann::json const& j, ScriptStatistics& s);
bool operator==(ScriptStatistics const& lhs, ScriptStatistics const& rhs);

struct TransactionInfo {
  std::string transaction_id;
};
void to_json(nlohmann::json& j, TransactionInfo const& t);
void from_json(nlohmann::json const& j, TransactionInfo& t);

struct DataMaskingStatistics {
  bool data_masking_applied = false;
};
void to_json(nlohmann::json& j, DataMaskingStatistics const& d);
void from_json(nlohmann::json const& j, DataMaskingStatistics& d);

struct RowLevelSecurityStatistics {
  bool row_level_security_applied = false;
};
void to_json(nlohmann::json& j, RowLevelSecurityStatistics const& r);
void from_json(nlohmann::json const& j, RowLevelSecurityStatistics& r);

struct SessionInfo {
  std::string session_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, SessionInfo const& s);
void from_json(nlohmann::json const& j, SessionInfo& s);

// Represents the statistics for single job execution.
// It can be used to get information about the job including
// start and end times.
//
// For more details, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#JobStatistics
struct JobStatistics {
  std::chrono::milliseconds creation_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds start_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds end_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds total_slot_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds final_execution_duration =
      std::chrono::milliseconds(0);

  std::int64_t total_bytes_processed = 0;
  std::int64_t num_child_jobs = 0;

  std::string parent_job_id;
  SessionInfo session_info;
  TransactionInfo transaction_info;
  std::string reservation_id;

  DataMaskingStatistics data_masking_statistics;
  RowLevelSecurityStatistics row_level_security_statistics;

  double completion_ratio = 0;
  std::vector<std::string> quota_deferments;

  ScriptStatistics script_statistics;
  JobQueryStatistics job_query_stats;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, JobStatistics const& s);
void from_json(nlohmann::json const& j, JobStatistics& s);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_STATS_H
