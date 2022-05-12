// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/internal/rowreaderiterator.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RowReaderIterator::RowReaderIterator(std::shared_ptr<RowReaderImpl> owner)
    : owner_(std::move(owner)) {
  Advance();
}

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
  auto variant = owner_->Advance();
  if (absl::holds_alternative<bigtable::Row>(variant)) {
    row_ = absl::get<bigtable::Row>(std::move(variant));
    return;
  }
  auto status = absl::get<Status>(std::move(variant));
  if (!status.ok()) {
    row_ = std::move(status);
    return;
  }
  // Successful end()
  owner_ = nullptr;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
