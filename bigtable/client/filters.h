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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_

#include "bigtable/client/version.h"

#include <google/bigtable/v2/data.pb.h>

#include <chrono>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the interfaces to create filter expressions.
 *
 * Example:
 * @code
 * // Get only data from the "fam" column family, and only the latest value.
 * auto filter = Filter::Chain(Filter::Family("fam"), Filter::Latest(1));
 * table->ReadRow("foo", std::move(filter));
 * @endcode
 *
 * Those filters that use regular expressions, expect the patterns to be in
 * the [RE2](https://github.com/google/re2/wiki/Syntax) syntax.
 *
 * @note Special care need be used with the expression used. Some of the
 *   byte sequences matched (e.g. row keys, or values), can contain arbitrary
 *   bytes, the `\C` escape sequence must be used if a true wildcard is
 *   desired. The `.` character will not match the new line character `\n`,
 *   effectively `.` means `[^\n]` in RE2.  As new line characters may be
 *   present in a binary value, you may need to explicitly match it using "\\n"
 *   the double escape is necessary because RE2 needs to get the escape
 *   sequence.
 */
class Filter {
 public:
  // TODO() - replace with `= default` if protobuf gets move constructors.
  Filter(Filter&& rhs) noexcept : Filter() { filter_.Swap(&rhs.filter_); }

  // TODO() - replace with `= default` if protobuf gets move constructors.
  Filter& operator=(Filter&& rhs) noexcept {
    Filter tmp(std::move(rhs));
    tmp.filter_.Swap(&filter_);
    return *this;
  }

  Filter(Filter const& rhs) = default;
  Filter& operator=(Filter const& rhs) = default;

  /// Return a filter that passes on all data.
  static Filter PassAllFilter() {
    Filter tmp;
    tmp.filter_.set_pass_all_filter(true);
    return tmp;
  }

  /// Return a filter that blocks all data.
  static Filter BlockAllFilter() {
    Filter tmp;
    tmp.filter_.set_block_all_filter(true);
    return tmp;
  }

  /**
   * Return a filter that accepts only the last @p n values of each column.
   *
   * TODO(#84) - document what is the effect of n <= 0
   */
  static Filter Latest(std::int32_t n) {
    Filter result;
    result.filter_.set_cells_per_column_limit_filter(n);
    return result;
  }

  /**
   * Return a filter that matches column families matching the given regexp.
   *
   * @param pattern the regular expression.  It must be a valid
   *     [RE2](https://github.com/google/re2/wiki/Syntax) pattern.
   *     For technical reasons, the regex must not contain the ':' character,
   *     even if it is not being used as a literal.
   */
  static Filter FamilyRegex(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_family_name_regex_filter(std::move(pattern));
    return tmp;
  }

  /**
   * Return a filter that accepts only columns matching the given regexp.
   *
   * @param pattern the regular expression.  It must be a valid
   *     [RE2](https://github.com/google/re2/wiki/Syntax) pattern.
   */
  static Filter ColumnRegex(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_column_qualifier_regex_filter(std::move(pattern));
    return tmp;
  }

  /**
   * Return a filter that accepts columns in the [@p start, @p end) range
   * within the @p family column family.
   */
  static Filter ColumnRange(std::string family, std::string start,
                            std::string end) {
    return ColumnRangeRightOpen(std::move(family), std::move(start),
                                std::move(end));
  }

  /**
   * Return a filter that accepts cells in the given timestamp range.
   *
   * The range is right-open, i.e., it represents [start, end).
   */
  static Filter TimestampRangeMicros(std::int64_t start, std::int64_t end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_timestamp_range_filter();
    range.set_start_timestamp_micros(start);
    range.set_end_timestamp_micros(end);
    return tmp;
  }

  /**
   * Return a filter that accepts cells in the given timestamp range.
   *
   * The range is right-open, i.e., it represents [start, end).
   *
   * The function accepts any instantiation of std::chrono::duration<> for the
   * @p start and @p end parameters.
   *
   * @tparam Rep1 the Rep tparam for @p start's type.
   * @tparam Period1 the Period tparam for @p start's type.
   * @tparam Rep2 the Rep tparam for @p end's type.
   * @tparam Period2 the Period tparam for @p end's type.
   */
  template <typename Rep1, typename Period1, typename Rep2, typename Period2>
  static Filter TimestampRange(std::chrono::duration<Rep1, Period1> start,
                               std::chrono::duration<Rep2, Period2> end) {
    using namespace std::chrono;
    return TimestampRangeMicros(duration_cast<microseconds>(start).count(),
                                duration_cast<microseconds>(end).count());
  }

  /**
   * Return a filter that matches keys matching the given regexp.
   *
   * @param pattern the regular expression.  It must be a valid RE2 pattern.
   *     More details at https://github.com/google/re2/wiki/Syntax
   */
  static Filter RowKeysRegex(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_row_key_regex_filter(std::move(pattern));
    return tmp;
  }

  /**
   * Return a filter that matches values matching the given regexp.
   *
   * @param pattern the regular expression.  It must be a valid
   *     [RE2](https://github.com/google/re2/wiki/Syntax) pattern.
   */
  static Filter ValueRegex(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_value_regex_filter(std::move(pattern));
    return tmp;
  }

  /// Return a filter matching values in the [@p start, @p end) range.
  static Filter ValueRange(std::string start, std::string end) {
    return ValueRangeRightOpen(std::move(start), std::move(end));
  }

  /**
   * Return a filter that only accepts the first @p n cells in a row.
   *
   * Notice that cells might be repeated, such as when interleaving the results
   * of multiple filters via the Union() function (aka Interleaved in the
   * proto).  Furthermore, notice that this is the cells within a row, if there
   * are multiple column families and columns, the cells are returned ordered
   * by first column family, and then by column qualifier, and then by
   * timestamp.
   *
   * TODO(#82) - check the documentation around ordering of columns.
   * TODO(#84) - document what is the effect of n <= 0
   */
  static Filter CellsRowLimit(std::int32_t n) {
    Filter tmp;
    tmp.filter_.set_cells_per_row_limit_filter(n);
    return tmp;
  }

  /**
   * Return a filter that skips the first @p n cells in a row.
   *
   * Notice that cells might be repeated, such as when interleaving the results
   * of multiple filters via the Union() function (aka Interleaved in the
   * proto).  Furthermore, notice that this is the cells within a row, if there
   * are multiple column families and columns, the cells are returned ordered
   * by first column family, and then by column qualifier, and then by
   * timestamp.
   *
   * TODO(#82) - check the documentation around ordering of columns.
   * TODO(#84) - document what is the effect of n <= 0
   */
  static Filter CellsRowOffset(std::int32_t n) {
    Filter tmp;
    tmp.filter_.set_cells_per_row_offset_filter(n);
    return tmp;
  }

  /**
   * Return a filter that samples rows with a given probability.
   *
   * TODO(#84) - decide what happens if the probability is out of range.
   *
   * @param probability the probability that any row will be selected.  It
   *     must be in the [0.0, 1.0] range.
   */
  static Filter RowSample(double probability) {
    Filter tmp;
    tmp.filter_.set_row_sample_filter(probability);
    return tmp;
  }

  //@{
  /**
   * @name Less common range filters.
   *
   * Cloud Bigtable range filters can include or exclude the limits of the
   * range.  In most cases applications use [start,end) ranges, and the
   * ValueRange() and ColumnRange() functions are offered to support the
   * common case.  For the less common cases where the application needs
   * different ranges the following functions are available.
   */
  /**
   * Return a filter that accepts values in the [@p start, @p end) range.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ValueRangeLeftOpen(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_open(std::move(start));
    range.set_end_value_closed(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts values in the [@p start, @p end] range.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ValueRangeRightOpen(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_closed(std::move(start));
    range.set_end_value_open(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts values in the [@p start, @p end] range.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ValueRangeClosed(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_closed(std::move(start));
    range.set_end_value_closed(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts values in the (@p start, @p end) range.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ValueRangeOpen(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_open(std::move(start));
    range.set_end_value_open(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts columns in the [@p start, @p end) range
   * within the @p column_family.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ColumnRangeRightOpen(std::string column_family,
                                     std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_column_range_filter();
    range.set_family_name(std::move(column_family));
    range.set_start_qualifier_closed(std::move(start));
    range.set_end_qualifier_open(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts columns in the (@p start, @p end] range
   * within the @p column_family.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ColumnRangeLeftOpen(std::string column_family,
                                    std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_column_range_filter();
    range.set_family_name(std::move(column_family));
    range.set_start_qualifier_open(std::move(start));
    range.set_end_qualifier_closed(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts columns in the [@p start, @p end] range
   * within the @p column_family.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ColumnRangeClosed(std::string column_family, std::string start,
                                  std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_column_range_filter();
    range.set_family_name(std::move(column_family));
    range.set_start_qualifier_closed(std::move(start));
    range.set_end_qualifier_closed(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts columns in the (@p start, @p end) range
   * within the @p column_family.
   *
   * TODO(#84) - document what happens if end < start
   */
  static Filter ColumnRangeOpen(std::string column_family, std::string start,
                                std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_column_range_filter();
    range.set_family_name(std::move(column_family));
    range.set_start_qualifier_open(std::move(start));
    range.set_end_qualifier_open(std::move(end));
    return tmp;
  }
  //@}

  //@{
  /// @name Unimplemented, wait for the next PR.
  // TODO(#30) - complete the implementation.
  static Filter SinkFilter();
  //@}

  /**
   * Return a filter that transforms any values into the empty string.
   *
   * As the name indicates, this acts as a transformer on the data, replacing
   * any values with the empty string.
   */
  static Filter StripValueTransformer() {
    Filter tmp;
    tmp.filter_.set_strip_value_transformer(true);
    return tmp;
  }

  /**
   * Returns a filter that applies a label to each value.
   *
   * Each value accepted by previous filters in modified to include the @p
   * label.
   *
   * @note Currently it is not possible to apply more than one label in a
   *     filter expression, that is, a chain can only contain a single
   *     ApplyLabelTransformer() filter.  This limitation may be lifted in
   *     the future.  It is possible to have multiple ApplyLabelTransformer
   *     filters in a Union() filter, though in this case each copy of a cell
   *     gets a different label.
   *
   * @param label the label applied to each cell.  The labels must be at most 15
   *     characters long, and must match the `[a-z0-9\\-]` pattern.
   * TODO(#84) - change this if we decide to validate inputs in the client side
   */
  static Filter ApplyLabelTransformer(std::string label) {
    Filter tmp;
    tmp.filter_.set_apply_label_transformer(std::move(label));
    return tmp;
  }
  //@}

  //@{
  /**
   * @name Compound filters.
   *
   * These filters compose several filters to build complex filter expressions.
   */
  /// Return a filter that selects between two other filters based on a
  /// predicate.
  // TODO(#30) - implement this one.
  static Filter Condition(Filter predicate, Filter true_filter,
                          Filter false_filter) {
    return Filter();
  }

  /// Create a chain of filters
  // TODO(coryan) - document ugly std::enable_if<> hack to ensure they all
  // are of type Filter.
  // TODO(#30) - implement this one.
  template <typename... FilterTypes>
  static Filter Chain(FilterTypes&&... a) {
    return Filter();
  }

  // TODO(coryan) - same ugly hack documentation needed ...
  // TODO(#30) - implement this one.
  /**
   * Return a filter that unions the results of all the other filters.
   * @tparam FilterTypes
   * @param a
   * @return
   */
  template <typename... FilterTypes>
  static Filter Union(FilterTypes&&... a) {
    return Filter();
  }
  //@}

  /// Return the filter expression as a protobuf.
  // TODO() consider a "move" operation too.
  google::bigtable::v2::RowFilter as_proto() const { return filter_; }

 private:
  /// An empty filter, discards all data.
  Filter() : filter_() {}

 private:
  google::bigtable::v2::RowFilter filter_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_
