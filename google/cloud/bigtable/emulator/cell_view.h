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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_CELL_VIEW_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_CELL_VIEW_H

#include <string>
#include <chrono>
#include <functional>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

/**
 * A class used to represent values when scanning a table.
 *
 * It is transient - it should never be stored as it only contains references to
 * data which will likely become invalidated on first update.
 */
class CellView {
 public:
  CellView(std::string const& row_key, std::string const& column_family,
           std::string const& column_qualifier,
           std::chrono::milliseconds timestamp, std::string const& value)
      : row_key_(row_key),
        column_family_(column_family),
        column_qualifier_(column_qualifier),
        timestamp_(timestamp),
        value_(value) {}

  std::string const& row_key() const { return row_key_.get(); }
  std::string const& column_family() const { return column_family_.get(); }
  std::string const& column_qualifier() const {
    return column_qualifier_.get();
  }
  std::chrono::milliseconds timestamp() const { return timestamp_; }
  std::string const& value() const { return value_.get(); }

 private:
  std::reference_wrapper<std::string const> row_key_;
  std::reference_wrapper<std::string const> column_family_;
  std::reference_wrapper<std::string const> column_qualifier_;
  std::chrono::milliseconds timestamp_;
  std::reference_wrapper<std::string const> value_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_CELL_VIEW_H
