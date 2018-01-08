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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ROWREADERITERATOR_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ROWREADERITERATOR_H_

#include "bigtable/client/version.h"

#include "bigtable/client/row.h"

#include <absl/types/optional.h>

#include <iterator>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// Forward declare the owner class of this iterator.
class RowReader;

namespace internal {
/**
 * The input iterator used to scan the rows in a RowReader.
 */
class RowReaderIterator : public std::iterator<std::input_iterator_tag, Row> {
 public:
  RowReaderIterator(RowReader* owner, bool is_end) : owner_(owner), row_() {}

  RowReaderIterator& operator++();
  RowReaderIterator operator++(int) {
    RowReaderIterator tmp(*this);
    operator++();
    return tmp;
  }

  Row const* operator->() const { return row_.operator->(); }
  Row* operator->() { return row_.operator->(); }

  Row const& operator*() const & { return row_.operator*(); }
  Row& operator*() & { return row_.operator*(); }
  Row const&& operator*() const && { return std::move(row_.operator*()); }
  Row&& operator*() && { return std::move(row_.operator*()); }

  bool operator==(RowReaderIterator const& that) const {
    // All non-end iterators are equal.
    return (owner_ == that.owner_) and (bool(row_) == bool(that.row_));
  }

  bool operator!=(RowReaderIterator const& that) const {
    return !(*this == that);
  }

 private:
  RowReader* owner_;
  absl::optional<Row> row_;
};
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ROWREADERITERATOR_H_
