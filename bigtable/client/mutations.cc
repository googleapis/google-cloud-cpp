// Copyright 2017 Google Inc.
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

#include "bigtable/client/mutations.h"
#include <google/protobuf/text_format.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
Mutation SetCell(std::string family, std::string column,
                 std::chrono::milliseconds timestamp, std::string value) {
  Mutation m;
  auto& set_cell = *m.op.mutable_set_cell();
  set_cell.set_family_name(std::move(family));
  set_cell.set_column_qualifier(std::move(column));
  set_cell.set_timestamp_micros(
      std::chrono::duration_cast<std::chrono::microseconds>(timestamp).count());
  set_cell.set_value(std::move(value));
  return m;
}

Mutation SetCell(std::string family, std::string column, std::string value) {
  Mutation m;
  auto& set_cell = *m.op.mutable_set_cell();
  set_cell.set_family_name(std::move(family));
  set_cell.set_column_qualifier(std::move(column));
  set_cell.set_timestamp_micros(ServerSetTimestamp());
  set_cell.set_value(std::move(value));
  return m;
}

Mutation DeleteFromColumn(std::string family, std::string column,
                          std::int64_t timestamp_begin,
                          std::int64_t timestamp_end) {
  Mutation m;
  auto& d = *m.op.mutable_delete_from_column();
  d.set_family_name(std::move(family));
  d.set_column_qualifier(std::move(column));
  d.mutable_time_range()->set_start_timestamp_micros(timestamp_begin);
  d.mutable_time_range()->set_end_timestamp_micros(timestamp_end);
  return m;
}

Mutation DeleteFromColumn(std::string family, std::string column) {
  Mutation m;
  auto& d = *m.op.mutable_delete_from_column();
  d.set_family_name(std::move(family));
  d.set_column_qualifier(std::move(column));
  return m;
}

Mutation DeleteFromColumnStartingFrom(std::string family, std::string column,
                                      std::int64_t timestamp_begin) {
  Mutation m;
  auto& d = *m.op.mutable_delete_from_column();
  d.set_family_name(std::move(family));
  d.set_column_qualifier(std::move(column));
  d.mutable_time_range()->set_start_timestamp_micros(timestamp_begin);
  return m;
}

Mutation DeleteFromColumnEndingAt(std::string family, std::string column,
                                  std::int64_t timestamp_end) {
  Mutation m;
  auto& d = *m.op.mutable_delete_from_column();
  d.set_family_name(std::move(family));
  d.set_column_qualifier(std::move(column));
  d.mutable_time_range()->set_end_timestamp_micros(timestamp_end);
  return m;
}

Mutation DeleteFromFamily(std::string family) {
  Mutation m;
  auto& d = *m.op.mutable_delete_from_family();
  d.set_family_name(std::move(family));
  return m;
}

Mutation DeleteFromRow() {
  Mutation m;
  (void)m.op.mutable_delete_from_row();
  return m;
}

grpc::Status FailedMutation::to_grpc_status(google::rpc::Status const& status) {
  std::string details;
  if (not google::protobuf::TextFormat::PrintToString(status, &details)) {
    details = "error [could not print details as string]";
  }
  return grpc::Status(static_cast<grpc::StatusCode>(status.code()),
                      status.message(), std::move(details));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
