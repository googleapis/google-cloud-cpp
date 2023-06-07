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

#include "google/cloud/bigquery/v2/minimal/internal/job_stats.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

EvaluationKind EvaluationKind::UnSpecified() {
  return EvaluationKind{"EVALUATION_KIND_UNSPECIFIED"};
}
EvaluationKind EvaluationKind::Statement() {
  return EvaluationKind{"STATEMENT"};
}
EvaluationKind EvaluationKind::Expression() {
  return EvaluationKind{"EXPRESSION"};
}

void to_json(nlohmann::json& j, JobStatistics const& s) {
  j = nlohmann::json{
      {"total_bytes_processed", s.total_bytes_processed},
      {"num_child_jobs", s.num_child_jobs},
      {"total_modified_partitions", s.total_modified_partitions},
      {"parent_job_id", s.parent_job_id},
      {"session_id", s.session_id},
      {"transaction_id", s.transaction_id},
      {"reservation_id", s.reservation_id},
      {"row_level_security_applied", s.row_level_security_applied},
      {"data_masking_applied", s.data_masking_applied},
      {"completion_ratio", s.completion_ratio},
      {"quota_deferments", s.quota_deferments},
      {"script_statistics", s.script_statistics},
      {"job_query_stats", s.job_query_stats}};

  ToJson(s.start_time, j, "start_time");
  ToJson(s.end_time, j, "end_time");
  ToJson(s.creation_time, j, "creation_time");
  ToJson(s.total_slot_time, j, "total_slot_time");
  ToJson(s.final_execution_duration, j, "final_execution_duration");
}

void from_json(nlohmann::json const& j, JobStatistics& s) {
  if (j.contains("total_bytes_processed"))
    j.at("total_bytes_processed").get_to(s.total_bytes_processed);
  if (j.contains("num_child_jobs"))
    j.at("num_child_jobs").get_to(s.num_child_jobs);
  if (j.contains("total_modified_partitions"))
    j.at("total_modified_partitions").get_to(s.total_modified_partitions);
  if (j.contains("parent_job_id"))
    j.at("parent_job_id").get_to(s.parent_job_id);
  if (j.contains("session_id")) j.at("session_id").get_to(s.session_id);
  if (j.contains("transaction_id"))
    j.at("transaction_id").get_to(s.transaction_id);
  if (j.contains("reservation_id"))
    j.at("reservation_id").get_to(s.reservation_id);
  if (j.contains("row_level_security_applied"))
    j.at("row_level_security_applied").get_to(s.row_level_security_applied);
  if (j.contains("data_masking_applied"))
    j.at("data_masking_applied").get_to(s.data_masking_applied);
  if (j.contains("completion_ratio"))
    j.at("completion_ratio").get_to(s.completion_ratio);
  if (j.contains("quota_deferments"))
    j.at("quota_deferments").get_to(s.quota_deferments);
  if (j.contains("script_statistics"))
    j.at("script_statistics").get_to(s.script_statistics);
  if (j.contains("job_query_stats"))
    j.at("job_query_stats").get_to(s.job_query_stats);

  FromJson(s.start_time, j, "start_time");
  FromJson(s.end_time, j, "end_time");
  FromJson(s.creation_time, j, "creation_time");
  FromJson(s.total_slot_time, j, "total_slot_time");
  FromJson(s.final_execution_duration, j, "final_execution_duration");
}

bool operator==(ScriptStackFrame const& lhs, ScriptStackFrame const& rhs) {
  return lhs.end_column == rhs.end_column && lhs.end_line == rhs.end_line &&
         lhs.procedure_id == rhs.procedure_id &&
         lhs.start_column == rhs.start_column &&
         lhs.start_line == rhs.start_line && lhs.text == rhs.text;
}

bool operator==(ScriptStatistics const& lhs, ScriptStatistics const& rhs) {
  auto const eq = lhs.evaluation_kind == rhs.evaluation_kind;
  return eq && (std::equal(lhs.stack_frames.begin(), lhs.stack_frames.end(),
                           rhs.stack_frames.begin()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
