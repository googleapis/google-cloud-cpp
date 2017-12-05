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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_

#include <bigtable/client/version.h>
#include <google/bigtable/v2/data.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the interfaces to create filter expressions.
 *
 * Example:
 * @code
 * // Get only data from the "fam" column family, and only the latest value.
 * auto filter = Filter::chain(Filter::family("fam"), Filter::lastest(1));
 * table->ReadRow("foo", std::move(filter));
 * @endcode
 */
class Filter {
 public:
  /// An empty filter, discards all data.
  Filter() : filter_() {}

  /// Create a filter that accepts only the last @a n values.
  static Filter latest(int n);

  google::bigtable::v2::RowFilter as_proto() {
    return filter_;
  }

 private:
  google::bigtable::v2::RowFilter filter_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif //GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_
