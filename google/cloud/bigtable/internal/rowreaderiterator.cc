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

#include "google/cloud/bigtable/internal/rowreaderiterator.h"
#include "google/cloud/bigtable/row_reader.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

RowReaderIterator::RowReaderIterator(RowReader* owner) : owner_(owner) {
  Advance();
}

RowReaderIterator::RowReaderIterator() : owner_() {}

// Defined here because it needs to see the definition of RowReader
RowReaderIterator& RowReaderIterator::operator++() {
  if (owner_ == nullptr) {
    // This is the `end()` iterator, using operator++ on it is UB, we can do
    // whatever we want, we chose to do nothing.
    return *this;
  }
  if (!row_) {
    // If the iterator dereferences to a bad status, the next value is end().
    owner_ = nullptr;
    return *this;
  }
  Advance();
  return *this;
}

void RowReaderIterator::Advance() {
  auto status_or_optional_row = owner_->Advance();
  if (!status_or_optional_row) {
    row_ = StatusOr<Row>(std::move(status_or_optional_row).status());
    return;
  }
  auto& optional_row = *status_or_optional_row;
  if (optional_row) {
    row_ = *std::move(optional_row);
    return;
  }
  // Successful end()
  owner_ = nullptr;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
