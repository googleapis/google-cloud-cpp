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

// The code declared in this file is used to construct filters according to
// `::google::bigtable::v2::RowFilter` protobuf definition.
// It describes a DAG through which cells should be routed (and potentially
// copied in case of the `Interleave` filter).
//
// The simplest way of implementing such a DAG is to create an object for every
// node of the graph, which would filter/transform the result. This, however,
// could be very inefficient. For example, if we're only interested in the last
// version of a cell of a specific column, the lowermost layers of the graph
// would have to scan the whole table.
//
// This example shows that we should apply the filters as close to the beginning
// of the graph as possible. The in-memory implementation could jump over
// uninteresting columns and avoid passing all the values around. Most of the
// filters can be applied in any order, which makes our filtering task easy.
//
// Unfortunately, some filters (e.g. `cells_per_row_limit_filter`) prevents us
// from moving filters applied later in the chain to its beginning. Hence, we
// need to keep the naive (object-per-graph-node) approach at least as a backup
// option.
//
// We do attempt to apply the filtering as close to the root as possible,
// though. It is performed via the `AbstractCellStreamImpl::Apply()` function.
// This operation has different implementations for different filters.
//
// The algorithm looks as follows:
// * we try to build the DAG according to the proto, from the ground up
// * every time we're about to add a new node, we first try applying the filter
//   to the graph we built so far by calling `Apply()` on the last node we
//   added;
// * these `Apply()` calls are propagated through the graph all the way to the
//   root
// * if the `Apply()` call fails (e.g. because there is a
//   `cells_per_row_limit_filter` in the DAG), we will continue with adding a
//   new node to the graph
// * if the `Apply` call succeeds then we know that the lower layers will filter
//   out the unwanted data, so we can skip adding the node to the graph.

/// Only return cells from rows whose keys match `regex`.
struct RowKeyRegex {
  std::shared_ptr<re2::RE2> regex;
};
/// Only return cells from column families whose names match `regex`.
struct FamilyNameRegex {
  std::shared_ptr<re2::RE2> regex;
};
/// Only return cells from columns whose qualifiers match `regex`.
struct ColumnRegex {
  std::shared_ptr<re2::RE2> regex;
};
/// Only return cells from columns which fall into `range`.
struct ColumnRange {
  std::string column_family;
  StringRangeSet::Range range;
};
/// Only return cells from timestamps which fall into `range`.
struct TimestampRange {
  TimestampRangeSet::Range range;
};

using InternalFilter = absl::variant<RowKeyRegex, FamilyNameRegex, ColumnRegex,
                                     ColumnRange, TimestampRange>;
enum class NextMode {
  // Advance a stream to the next available cell.
  kCell = 0,
  // Advance a stream to the first cell which is in a different column.
  kColumn,
  // Advance a stream to the first cell which is in a different row.
  kRow,
};

/**
 * An interface for `CellView` stream implementations.
 *
 * Objects of classes implementing this abstract class represent a stream of
 * `CellView`. They should all guarantee that returned `CellViews` are sorted by
 * (row_key, column_family, column_qualifier, timestamp).
 *
 * Depending on the implementation, objects of this class may support filtering
 * of the returned `CellView`. The users may request filtering via `Apply()`. It
 * should be used only before first access to the actual stream (i.e. functions
 * `HasValue()`, `Value()` and `Next()`).
 *
 * Objects of derived classes should be assumed to be not thread safe.
 */
class AbstractCellStreamImpl {
 public:
  virtual ~AbstractCellStreamImpl() = default;

  /**
   * Attempt to apply a filter on the stream.
   *
   * It should not be called after `HasValue()`, `Value()` or `Next()` have been
   * called.
   *
   * Depending on the implementation the application may succeed or not. If it
   * doesn't, the stream is unchanged.
   *
   * @param internal_filter a filter to apply on the stream.
   * @return whether the filter application succeeded. If it didn't the filter
   *     is unchanged.
   */
  virtual bool ApplyFilter(InternalFilter const& internal_filter) = 0;
  /// Whether the stream is pointing to a cell or has it finished.
  virtual bool HasValue() const = 0;
  /**
   * The first "unconsumed" value.
   *
   * \pre{One should not call this member function if `HasValue() == false`.}
   *
   * @return currently pointed cell
   */
  virtual CellView const& Value() const = 0;
  /**
   * Advance the stream to next `CellView`.
   *
   * \pre{One should not call this member function if `HasValue() == false`.}
   *
   * Specific implementations have to support `mode == NextMode::kCell` but may
   * not support others. If the requested `mode` is not supported, `false` is
   * returned.
   *
   * @param mode how far to advance - it may be any next cell, or the first cell
   *     which is in a different column or the first cell which is in a
   *     different row.
   * @return whether `mode` is supported; the returned value is unrelated to
   *     what `HasValue()` will return.
   */
  virtual bool Next(NextMode mode) = 0;
};

/**
 * A convenience wrapper around `AbstractCellStreamImpl`.
 *
 * The purpose of this class is to provide what `AbstractCellStreamImpl`
 * implementations do but with a more convenient interface.
 */
class CellStream {
 public:
  explicit CellStream(std::unique_ptr<AbstractCellStreamImpl> impl)
      : impl_(std::move(impl)) {}

  /**
   * Attempt to apply a filter on the stream.
   *
   * It should not be called after `HasValue()`, `Value()` or `Next()` have been
   * called.
   *
   * Depending on the implementation the application may succeed or not. If it
   * doesn't, the stream is unchanged.
   *
   * @param internal_filter a filter to apply on the stream.
   * @return whether the filter application succeeded. If it didn't the filter
   *     is unchanged.
   */
  bool ApplyFilter(InternalFilter const& internal_filter) {
    return impl_->ApplyFilter(internal_filter);
  }
  /// Whether the stream is pointing to a cell or has it finished.
  bool HasValue() const { return impl_->HasValue(); }
  /**
   * The first "unconsumed" value.
   *
   * \pre{One should not call this member function if `HasValue() == false`.}
   *
   * @return currently pointed cell
   */
  CellView const& Value() const { return impl_->Value(); }
  /**
   * Advance the stream to next `CellView`.
   *
   * \pre{One should not call this member function if `HasValue() == false`.}
   *
   * @param mode how far to advance - it may be any next cell, or the first cell
   *     which is in a different column or the first cell which is in a
   *     different row.
   */
  void Next(NextMode mode = NextMode::kCell);
  /// equivalent to `Next(NextMode::kCell)`
  void operator++() { Next(); }
  /// equivalent to `Next(NextMode::kCell)`
  CellView operator++(int);
  CellView const& operator*() const { return Value(); }
  CellView const* operator->() const { return &Value(); }
  /// equivalent to `HasValue()`
  explicit operator bool() const { return HasValue(); }
  AbstractCellStreamImpl const& impl() const { return *impl_; }

 private:
  void NextColumn();
  void EmulateNextColumn();
  void EmulateNextRow();
  std::unique_ptr<AbstractCellStreamImpl> impl_;
};

/**
 * A stream which merges multiple stream while maintaining ordering.
 */
class MergeCellStreams : public AbstractCellStreamImpl {
 public:
  class CellStreamGreater {
   public:
    bool operator()(std::unique_ptr<CellStream> const& lhs,
                    std::unique_ptr<CellStream> const& rhs) const;
  };

  explicit MergeCellStreams(std::vector<CellStream> streams);
  bool ApplyFilter(InternalFilter const& internal_filter) override;
  bool HasValue() const override;
  CellView const& Value() const override;
  bool Next(NextMode mode) override;

 private:
  void InitializeIfNeeded() const;

  mutable bool initialized_{false};

 protected:
  // A priority queue of streams which still have data.
  // `std::priority_queue` can't be used because it cannot be iterated over.
  mutable std::vector<std::unique_ptr<CellStream>> unfinished_streams_;
};

/**
 * Create a filter hierarchy according to a protobuf description.
 *
 * The filter hierarchy is essentially a DAG with specific filters in nodes.
 *
 * @param filter the protobuf description of the filter hierarchy
 * @param source_ctor a zero argument function to create the unfiltered stream
 *     to be filtered. Depending on `filter` it may be called multiple times and
 *     it should return separate streams each time.
 * @return the filtered stream or an error.
 */
using CellStreamConstructor = std::function<CellStream()>;
StatusOr<CellStream> CreateFilter(
    ::google::bigtable::v2::RowFilter const& filter,
    CellStreamConstructor source_ctor);

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTER_H
