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

#include "google/cloud/bigquery/v2/minimal/internal/table_constraints.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string PrimaryKey::DebugString(absl::string_view name,
                                    TracingOptions const& options,
                                    int indent) const {
  // DebugFormatter does not support std::vector<std::string> currently.
  // Uncomment the `columns` value once the support is available.
  return internal::DebugFormatter(name, options, indent)
      .Field("num_columns", columns.size())
      // .Field("columns", columns)
      .Build();
}

std::string ColumnReference::DebugString(absl::string_view name,
                                         TracingOptions const& options,
                                         int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("referencing_column", referencing_column)
      .StringField("referenced_column", referenced_column)
      .Build();
}

std::string ForeignKey::DebugString(absl::string_view name,
                                    TracingOptions const& options,
                                    int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("key_name", key_name)
      .SubMessage("referenced_table", referenced_table)
      .Field("column_references", column_references)
      .Build();
}

std::string TableConstraints::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("primary_key", primary_key)
      .Field("foreign_keys", foreign_keys)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
