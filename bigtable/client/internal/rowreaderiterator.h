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

#include "bigtable/client/internal/throw_delegate.h"
#include "bigtable/client/row.h"
#include <iterator>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// Forward declare the owner class of this iterator.
class RowReader;

namespace internal {
/**
 * A poor's man version of std::optional<Row>.
 *
 * This project needs to support C++11 and C++14, so std::optional<> is not
 * available.  We cannot use Abseil either, see #232 for the reasons.
 * So we implement a very minimal "OptionalRow" class that documents the intent
 * and we will remove it when possible.
 *
 * TODO(#277) - replace with absl::optional<> or std::optional<> when possible.
 */
class OptionalRow {
 public:
  OptionalRow() : row_(std::string(), {}), has_row_(false) {}

  Row* get() { return &row_; }
  Row const* get() const { return &row_; }

  Row& value() {
    if (not has_row_) {
      RaiseLogicError("access unset OptionalRow");
    }
    return row_;
  }
  Row const& value() const {
    if (not has_row_) {
      RaiseLogicError("access unset OptionalRow");
    }
    return row_;
  }

  operator bool() const { return has_row_; }
  bool has_value() const { return has_row_; }

  void reset() { has_row_ = false; }
  void emplace(Row&& row) {
    has_row_ = true;
    row_ = std::move(row);
  }

 private:
  Row row_;
  bool has_row_;
};

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

  Row const* operator->() const { return row_.get(); }
  Row* operator->() { return row_.get(); }

  Row const& operator*() const & { return row_.value(); }
  Row& operator*() & { return row_.value(); }
  Row const&& operator*() const && { return std::move(row_.value()); }
  Row&& operator*() && { return std::move(row_.value()); }

  bool operator==(RowReaderIterator const& that) const {
    // All non-end iterators are equal.
    return owner_ == that.owner_ and row_.has_value() == that.row_.has_value();
  }

  bool operator!=(RowReaderIterator const& that) const {
    return !(*this == that);
  }

 private:
  RowReader* owner_;
  OptionalRow row_;
};
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ROWREADERITERATOR_H_
