// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MOCK_ROW_READER_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MOCK_ROW_READER_IMPL_H

#include "google/cloud/bigtable/internal/row_reader_impl.h"
#include <iterator>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockRowReaderImpl : public RowReaderImpl {
 public:
  explicit MockRowReaderImpl(std::vector<StatusOr<bigtable::Row>> rows)
      : rows_(std::move(rows)), iter_(rows_.cbegin()), end_(rows_.cend()) {}

  ~MockRowReaderImpl() override = default;

  /// Skips remaining rows and invalidates current iterator.
  void Cancel() override { iter_ = end_; };

  StatusOr<OptionalRow> Advance() override {
    if (iter_ == end_) return make_status_or<OptionalRow>(absl::nullopt);
    auto sor = *iter_++;
    if (!sor) return sor.status();
    return make_status_or(absl::make_optional(*sor));
  }

 private:
  std::vector<StatusOr<bigtable::Row>> const rows_;
  using iter = std::vector<StatusOr<bigtable::Row>>::const_iterator;
  iter iter_;
  iter end_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MOCK_ROW_READER_IMPL_H
