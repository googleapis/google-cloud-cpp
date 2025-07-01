// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_TEST_UTIL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_TEST_UTIL_H

#include "google/cloud/bigtable/emulator/table.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

struct SetCellParams {
  std::string column_family_name;
  std::string column_qualifier;
  int64_t timestamp_micros;
  std::string data;
};

Status SetCells(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& table_name, std::string const& row_key,
    std::vector<SetCellParams>& set_cell_params);

StatusOr<std::shared_ptr<Table>> CreateTable(
    std::string const& table_name, std::vector<std::string>& column_families);

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_TEST_UTIL_H
