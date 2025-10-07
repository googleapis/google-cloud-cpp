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

#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <algorithm>
#include <iterator>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
bigtable::QueryRow QueryRowFriend::MakeQueryRow(
    std::vector<bigtable::Value> values,
    std::shared_ptr<std::vector<std::string> const> columns) {
  return bigtable::QueryRow(std::move(values), std::move(columns));
}
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal

namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

QueryRow::QueryRow()
    : QueryRow({}, std::make_shared<std::vector<std::string>>()) {}

QueryRow::QueryRow(std::vector<Value> values,
                   std::shared_ptr<std::vector<std::string> const> columns)
    : values_(std::move(values)), columns_(std::move(columns)) {
  if (values_.size() != columns_->size()) {
    GCP_LOG(FATAL) << "QueryRow's value and column sizes do not match: "
                   << values_.size() << " vs " << columns_->size();
  }
}

StatusOr<Value> QueryRow::get(std::size_t pos) const {
  if (pos < values_.size()) return values_[pos];
  return internal::InvalidArgumentError("position out of range",
                                        GCP_ERROR_INFO());
}

StatusOr<Value> QueryRow::get(std::string const& name) const {
  auto it = std::find(columns_->begin(), columns_->end(), name);
  if (it != columns_->end()) return get(std::distance(columns_->begin(), it));
  return internal::InvalidArgumentError("column name not found",
                                        GCP_ERROR_INFO());
}

bool operator==(QueryRow const& a, QueryRow const& b) {
  return a.values_ == b.values_ && *a.columns_ == *b.columns_;
}

//
// RowStreamIterator
//

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
