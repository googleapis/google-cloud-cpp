// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/row_stream.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RowStreamIterator::RowStreamIterator() = default;

RowStreamIterator::RowStreamIterator(Source source)
    : source_(std::move(source)) {
  ++*this;
}

RowStreamIterator& RowStreamIterator::operator++() {
  if (!row_ok_) {
    source_ = nullptr;  // Last row was an error; become "end"
    return *this;
  }
  row_ = source_();
  row_ok_ = row_.ok();
  if (row_ && row_->size() == 0) {
    source_ = nullptr;  // No more Rows to consume; become "end"
    return *this;
  }
  return *this;
}

RowStreamIterator RowStreamIterator::operator++(int) {
  auto old = *this;
  ++*this;
  return old;
}

bool operator==(RowStreamIterator const& a, RowStreamIterator const& b) {
  // Input iterators may only be compared to (copies of) themselves and end.
  // See https://en.cppreference.com/w/cpp/named_req/InputIterator. Therefore,
  // by definition, all input iterators are equal unless one is end and the
  // other is not.
  return !a.source_ == !b.source_;
}

bool operator!=(RowStreamIterator const& a, RowStreamIterator const& b) {
  return !(a == b);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
