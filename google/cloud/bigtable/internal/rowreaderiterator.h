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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROWREADERITERATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROWREADERITERATOR_H

#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/throw_delegate.h"
#include <iterator>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
// Forward declare the owner class of this iterator.
class RowReader;

namespace internal {

/**
 * The input iterator used to scan the rows in a RowReader.
 */
class RowReaderIterator {
 public:
  //@{
  /// @name Iterator traits
  using iterator_category = std::input_iterator_tag;
  using value_type = StatusOr<Row>;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  //@}

  explicit RowReaderIterator(RowReader* owner);
  RowReaderIterator() = default;

  RowReaderIterator& operator++();
  RowReaderIterator operator++(int) {
    RowReaderIterator tmp(*this);
    operator++();
    return tmp;
  }

  value_type const* operator->() const { return &row_; }
  value_type* operator->() { return &row_; }

  value_type const& operator*() const& { return row_; }
  value_type& operator*() & { return row_; }
  value_type const&& operator*() const&& { return std::move(row_); }
  value_type&& operator*() && { return std::move(row_); }

 private:
  friend bool operator==(RowReaderIterator const& lhs,
                         RowReaderIterator const& rhs);

  void Advance();
  /// nullptr indicates end()
  RowReader* owner_{};
  /// Current value of the iterator.
  StatusOr<Row> row_;
};

inline bool operator==(RowReaderIterator const& lhs,
                       RowReaderIterator const& rhs) {
  // All non-end iterators are equal.
  return lhs.owner_ == rhs.owner_;
}

inline bool operator!=(RowReaderIterator const& lhs,
                       RowReaderIterator const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROWREADERITERATOR_H
