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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROWREADERITERATOR_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROWREADERITERATOR_H_

#include "google/cloud/bigtable/row.h"
#include "google/cloud/internal/optional.h"
#include "google/cloud/internal/throw_delegate.h"
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// Forward declare the owner class of this iterator.
class RowReader;

namespace internal {
/**
 * An optional row value.
 *
 * TODO(#277) - replace with absl::optional<> or std::optional<> when possible.
 */
using OptionalRow = google::cloud::internal::optional<Row>;

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

  Row const& operator*() const& { return *row_; }
  Row& operator*() & { return *row_; }
#if !defined(__GNUC__) || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8)
  // Exclude this function for gcc-4.8. While we do not support gcc-4.8, we have
  // made an exception here because the TensorFlow folks need it.
  Row const&& operator*() const&& { return *std::move(row_); }
#endif  // !defined(__GNUC__) || __GNUC__ > 4 || (__GNUC__ == 4 &&
        // __GNUC_MINOR__ > 8)
  Row&& operator*() && { return *std::move(row_); }
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
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROWREADERITERATOR_H_
