// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTER_H

#include "google/cloud/bigtable/emulator/cell_view.h"
#include "google/cloud/stream_range.h"
#include <google/bigtable/v2/data.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {


struct RowKeyRegex {
  std::string regex;
};
struct FamilyNameRegex {
  std::string regex;
};
struct ColumnRegex {
  std::string regex;
};

using InternalFilter = absl::variant<google::bigtable::v2::ColumnRange,
                                     google::bigtable::v2::TimestampRange,
                                     RowKeyRegex, FamilyNameRegex, ColumnRegex>;

class AbstractCellStreamImpl {
 public:
  virtual ~AbstractCellStreamImpl() = default;

  virtual bool ApplyFilter(InternalFilter const& internal_filter) = 0;
  virtual absl::optional<CellView> Next() = 0;
  // Make sure Next() returns a different row than on last invocation. Noop in
  // Next() was never called before.
  virtual bool SkipRow() = 0;
  // Make sure Next() returns a different (row, column) pair than on last
  // invocation. Noop in Next() was never called before.
  virtual bool SkipColumn() = 0;
};

class CellStream {
 public:
  CellStream(std::shared_ptr<AbstractCellStreamImpl> impl)
      : impl_(std::move(impl)), current_(impl_->Next()) {}

  bool HasValue() const { return current_.has_value(); }
  CellView const & Value() const { return *current_; }
  void Next() { current_ = impl_->Next(); }
  bool SkipColumn() { return impl_->SkipColumn(); }
  bool SkipRow() { return impl_->SkipRow(); }
  void operator++();
  CellView operator++(int);
  CellView operator*() const { return Value(); }
  CellView const* operator->() const { return &Value(); }
  explicit operator bool() const { return HasValue(); }

 private:
  std::shared_ptr<AbstractCellStreamImpl> impl_;
  absl::optional<CellView> current_;
};

class FilterContext {
 public:
  FilterContext() : allow_apply_label_(true) {}

  FilterContext& DisallowApplyLabel();

  bool IsApplyLabelAllowed() const { return allow_apply_label_; }
 private:
  bool allow_apply_label_;
};

CellStream JoinCellStreams(std::vector<CellStream> cell_streams);

StatusOr<CellStream> CreateFilter(
    ::google::bigtable::v2::RowFilter const& filter, CellStream source,
    FilterContext const& ctx);

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTER_H

