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
#include "google/cloud/bigtable/emulator/range_set.h"
#include <google/bigtable/v2/data.pb.h>

namespace re2 {
class RE2;
}  // namespace re2

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {


struct RowKeyRegex {
  std::shared_ptr<re2::RE2> regex;
};
struct FamilyNameRegex {
  std::shared_ptr<re2::RE2> regex;
};
struct ColumnRegex {
  std::shared_ptr<re2::RE2> regex;
};
struct ColumnRange {
  StringRangeSet::Range range;
};
struct TimestampRange {
  TimestampRangeSet::Range range;
};

using InternalFilter = absl::variant<RowKeyRegex, FamilyNameRegex, ColumnRegex,
                                     ColumnRange, TimestampRange>;
enum class NextMode {
  kCell = 0,
  kColumn,
  kRow,
};

class AbstractCellStreamImpl {
 public:
  virtual ~AbstractCellStreamImpl() = default;

  virtual bool ApplyFilter(InternalFilter const& internal_filter) = 0;
  virtual bool HasValue() const = 0;
  virtual CellView const &Value() const = 0;
  virtual bool Next(NextMode mode = NextMode::kCell) = 0;
};

class CellStream {
 public:
  CellStream(std::unique_ptr<AbstractCellStreamImpl> impl)
      : impl_(std::move(impl)) {}

  bool ApplyFilter(InternalFilter const& internal_filter) {
    return impl_->ApplyFilter(internal_filter);
  }
  bool HasValue() const { return impl_->HasValue(); }
  CellView const & Value() const { return impl_->Value(); }
  void Next(NextMode mode = NextMode::kCell);
  void operator++() { Next(); }
  CellView operator++(int);
  CellView const& operator*() const { return Value(); }
  CellView const* operator->() const { return &Value(); }
  explicit operator bool() const { return HasValue(); }
  AbstractCellStreamImpl const &impl() const { return *impl_; }

 private:
  std::unique_ptr<AbstractCellStreamImpl> impl_;
};

class FilterContext {
 public:
  FilterContext() : allow_apply_label_(true) {}

  FilterContext& DisallowApplyLabel();

  bool IsApplyLabelAllowed() const { return allow_apply_label_; }
 private:
  bool allow_apply_label_;
};

class MergeCellStreams : public AbstractCellStreamImpl {
 public:
  class CellStreamGreater {
   public:
    bool operator()(std::unique_ptr<CellStream> const& lhs,
                    std::unique_ptr<CellStream> const& rhs) const;
  };

  MergeCellStreams(std::vector<CellStream> streams);
  bool ApplyFilter(InternalFilter const& internal_filter) override;
  bool HasValue() const override;
  CellView const& Value() const override;
  bool Next(NextMode mode) override;

 private:
  void InitializeIfNeeded() const;
  void ReassesStreams() const;
  bool SkipRowOrColumn(NextMode mode);

  mutable bool initialized_;
 protected:
  // A priority queue of streams which still have data.
  // `std::priority_queue` can't be used because it cannot be iterated over.
  mutable std::vector<std::unique_ptr<CellStream>> unfinished_streams_;
};

CellStream JoinCellStreams(std::vector<CellStream> cell_streams);

using CellStreamConstructor = std::function<CellStream()>;
StatusOr<CellStream> CreateFilter(
    ::google::bigtable::v2::RowFilter const& filter,
    CellStreamConstructor source_ctor, FilterContext const& ctx);

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTER_H

