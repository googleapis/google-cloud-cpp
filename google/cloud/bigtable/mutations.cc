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

#include "google/cloud/bigtable/mutations.h"
#include <google/protobuf/text_format.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
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

Mutation SetCell(Cell cell) {
  Mutation m;
  auto& set_cell = *m.op.mutable_set_cell();
  set_cell.set_family_name(std::move(cell.family_name_));
  set_cell.set_column_qualifier(std::move(cell.column_qualifier_));
  set_cell.set_timestamp_micros(cell.timestamp_);
  set_cell.set_value(std::move(cell.value_));
  return m;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
