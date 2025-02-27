// Copyright 2024 Google LLC
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

#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

std::string DumpStream(
    AbstractCellStreamImpl& stream, NextMode next_mode = NextMode::kCell) {
  std::stringstream ss;
  for (; stream.HasValue(); stream.Next(next_mode)) {
    auto const& cell = stream.Value();
    ss << cell.row_key() << " " << cell.column_family() << ":"
       << cell.column_qualifier() << " @" << cell.timestamp().count()
       << "ms: " << cell.value() << std::endl;
  }
  return ss.str();
}


}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
