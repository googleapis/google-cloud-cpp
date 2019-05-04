// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_KEY_SAMPLE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_KEY_SAMPLE_H_

#include "google/cloud/bigtable/version.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/// A simple wrapper to represent the response from `Table::SampleRowKeys()`.
struct RowKeySample {
  /**
   * A row key value strictly larger than all the rows included in this sample.
   *
   * Note that the service may return row keys that do not exist in the Cloud
   * Bigtable table. This should be interpreted as "a split point for sharding
   * a `Table::ReadRows()` call.  That is calling `Table::ReadRows()` to return
   * all the rows in the range `[previous-sample-row-key, this-sample-row-key)`
   * is expected to produce an efficient sharding of the `Table::ReadRows()`
   * operation.
   */
  std::string row_key;

  /// An estimate of the table size for all the rows smaller than `row_key`.
  std::int64_t offset_bytes;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_KEY_SAMPLE_H_
