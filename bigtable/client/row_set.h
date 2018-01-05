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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_SET_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_SET_H_

#include "bigtable/client/version.h"

#include <google/bigtable/v2/data.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Temporary definition of the RowSet class.
 *
 * TODO(#31): Replace all this with a full implementation.
 */
class RowSet {
 public:
  /// All rows.
  RowSet() : row_set_() {}

  /// Return as a protobuf.
  ::google::bigtable::v2::RowSet as_proto() const { return row_set_; }

  /// Move out the underlying protobuf value.
  ::google::bigtable::v2::RowSet as_proto_move() { return std::move(row_set_); }

 private:
  google::bigtable::v2::RowSet row_set_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_SET_H_
