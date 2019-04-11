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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_FILTERS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_FILTERS_H_

#include "google/cloud/bigtable/internal/conjunction.h"
#include "google/cloud/bigtable/version.h"
#include <google/bigtable/v2/data.pb.h>
#include <chrono>

namespace google {
namespace cloud {
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
  Filter(Filter&& rhs) noexcept = default;
  Filter& operator=(Filter&& rhs) noexcept = default;
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
   * The server rejects filters where @p n <= 0, any ReadRows() request
   * containing such a filter fails with `grpc::StatusCode::INVALID_ARGUMENT`.
   * This function does not perform any local validation of @p n.
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
   *     [RE2](https://github.com/google/re2/wiki/Syntax) pattern.  For
   *     technical reasons, the regex must not contain the ':' character, even
   *     if it is not being used as a literal. The server rejects filters with
   *     invalid patterns, including patterns containing the ':' character.
   *     The server fails the ReadRows() request with a
   *     `grpc::StatusCode::INVALID_ARGUMENT` status code. This function makes
   *     no attempt to validate the pattern before sending it to the server.
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
   *     [RE2](https://github.com/google/re2/wiki/Syntax) pattern. The server
   *     rejects filters with an invalid pattern with a
   *     `grpc::StatusCode::INVALID_ARGUMENT` status code.  This function makes
   *     no attempt to validate the pattern before sending it to the server.
   */
  static Filter ColumnRegex(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_column_qualifier_regex_filter(std::move(pattern));
    return tmp;
  }

  /**
   * Return a filter that accepts columns in the range [@p start, @p end)
   * within the @p family column family.
   *
   * The column range must be non-empty, i.e., @p start must be strictly
   * smaller than  @p end.  The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` status code. This function makes no
   * attempt to validate the column family or column range before sending them
   * to the server.
   */
  static Filter ColumnRange(std::string family, std::string start,
                            std::string end) {
    return ColumnRangeRightOpen(std::move(family), std::move(start),
                                std::move(end));
  }

  /**
   * Return the filter that accepts the named @p column within the @p family
   * column family.
   *
   * This function makes no attempt to validate the column family or column
   * range before sending them to the server.
   */
  static Filter ColumnName(std::string family, std::string column) {
    std::string end = column;
    return ColumnRangeClosed(std::move(family), std::move(column),
                             std::move(end));
  }

  /**
   * Return a filter that accepts cells with timestamps in the range
   * [@p start, @p end).
   *
   * The timestamp range must be non-empty, i.e. @p start must be strictly
   * smaller than  @p end.  The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` status code. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
   */
  static Filter TimestampRangeMicros(std::int64_t start, std::int64_t end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_timestamp_range_filter();
    range.set_start_timestamp_micros(start);
    range.set_end_timestamp_micros(end);
    return tmp;
  }

  /**
   * Return a filter that accepts cells with timestamps in the range
   * [@p start, @p end).
   *
   * The function accepts any instantiation of `std::chrono::duration<>` for the
   * @p start and @p end parameters.  For example:
   *
   * @code
   * using namespace std::chrono_literals; // C++14
   * auto r1 = bigtable::Filter::TimestampRange(10ms, 500ms);
   * auto r2 = bigtable::Filter::TimestampRange(10min, 10min + 2s);
   * @endcode
   *
   * The timestamp range must be non-empty, i.e. @p start must be strictly
   * smaller than  @p end.  The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
   *
   * @tparam Rep1 a placeholder to match the Rep tparam for @p start type,
   *     the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the underlying arithmetic type
   *     used to store the number of ticks), for our purposes it is simply a
   *     formal parameter.
   * @tparam Rep2 similar formal parameter for the type of @p end.
   * @tparam Period1 a placeholder to match the Period tparam for @p start
   *     type, the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the length of the tick in seconds,
   *     expressed as a `std::ratio<>`), for our purposes it is simply a formal
   *     parameter.
   * @tparam Period2 similar formal parameter for the type of @p end.
   *
   * @see
   * [std::chrono::duration<>](http://en.cppreference.com/w/cpp/chrono/duration)
   *     for more details.
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
   * Return a filter that matches cells with values matching the given regexp.
   *
   * @param pattern the regular expression.  It must be a valid
   *     [RE2](https://github.com/google/re2/wiki/Syntax) pattern. The server
   *     rejects filters with an invalid pattern with a
   *     `grpc::StatusCode::INVALID_ARGUMENT` status code. This function makes
   *     no attempt to validate the timestamp range before sending it to
   *     the server.
   */
  static Filter ValueRegex(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_value_regex_filter(std::move(pattern));
    return tmp;
  }

  /**
   * Return filter matching values in the range [@p start, @p end).
   *
   * @see ValueRangeRightOpen() for more details.
   */
  static Filter ValueRange(std::string start, std::string end) {
    return ValueRangeRightOpen(std::move(start), std::move(end));
  }

  /**
   * Return a filter that only accepts the first @p n cells in a row.
   *
   * Note that cells might be repeated, such as when interleaving the results
   * of multiple filters via the `Interleave()` function.  Furthermore, this
   * filter apples to the cells within a row; if there are multiple column
   * families and/or columns in a row, the order is:
   * - All the cells for a column family appear together, but there is no
   *   guarantee on the order of the column families.  Furthermore, column
   *   families may appear in different orders in different rows.
   * - Within a column family, the cells are ordered by column name, where
   *   column names are sorted lexicographically.
   * - Within a column, the cells appear in descending order by timestamp.
   *
   * The server rejects filters where @p n <= 0, any ReadRows() request
   * containing such a filter fails with `grpc::StatusCode::INVALID_ARGUMENT`.
   * This function does not perform any local validation of @p n.
   */
  static Filter CellsRowLimit(std::int32_t n) {
    Filter tmp;
    tmp.filter_.set_cells_per_row_limit_filter(n);
    return tmp;
  }

  /**
   * Return a filter that skips the first @p n cells in a row.
   *
   * Note that cells might be repeated, such as when interleaving the results
   * of multiple filters via the `Interleave()` function.  Furthermore, this
   * filter apples to the cells within a row; if there are multiple column
   * families and/or columns in a row, the order is:
   * - All the cells for a column family appear together, but there is no
   *   guarantee on the order of the column families.  Furthermore, column
   *   families may appear in different orders in different rows.
   * - Within a column family, the cells are ordered by column name, where
   *   column names are sorted lexicographically.
   * - Within a column, the cells appear in descending order by timestamp.
   *
   * The server rejects filters where @p n <= 0, any ReadRows() request
   * containing such a filter fails with `grpc::StatusCode::INVALID_ARGUMENT`.
   * This function does not perform any local validation of @p n.
   */
  static Filter CellsRowOffset(std::int32_t n) {
    Filter tmp;
    tmp.filter_.set_cells_per_row_offset_filter(n);
    return tmp;
  }

  /**
   * Return a filter that samples rows with a given probability.
   *
   * The server rejects filters where @p probability is outside the range
   * (0.0, 1.0).  Any ReadRows() request containing such a filter fails with
   * `grpc::StatusCode::INVALID_ARGUMENT`. This function does not perform any
   * local validation of @p probability.
   *
   * @param probability the probability that any row will be selected.  It
   *     must be in the range (0.0, 1.0).
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
   * range.  In most cases applications use [@p start, @p end) ranges, and the
   * ValueRange() and ColumnRange() functions are offered to support the
   * common case.  For the less common cases where the application needs
   * different ranges, the following functions are available.
   */
  /**
   * Return a filter that accepts values in the range [@p start, @p end).
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
   */
  static Filter ValueRangeLeftOpen(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_open(std::move(start));
    range.set_end_value_closed(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts values in the range [@p start, @p end].
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
   */
  static Filter ValueRangeRightOpen(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_closed(std::move(start));
    range.set_end_value_open(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts values in the range [@p start, @p end].
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
   */
  static Filter ValueRangeClosed(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_closed(std::move(start));
    range.set_end_value_closed(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts values in the range (@p start, @p end).
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
   */
  static Filter ValueRangeOpen(std::string start, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_open(std::move(start));
    range.set_end_value_open(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts columns in the range [@p start, @p end)
   * within the @p column_family.
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
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
   * Return a filter that accepts columns in the range (@p start, @p end]
   * within the @p column_family.
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
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
   * Return a filter that accepts columns in the range [@p start, @p end]
   * within the @p column_family.
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
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
   * Return a filter that accepts columns in the range (@p start, @p end)
   * within the @p column_family.
   *
   * The range must be non-empty. The server will reject empty ranges with a
   * `grpc::StatusCode::INVALID_ARGUMENT` error. This function makes no
   * attempt to validate the timestamp range before sending it to the server.
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
   * @note Currently, it is not possible to apply more than one label in a
   *     filter expression, that is, a chain can only contain a single
   *     ApplyLabelTransformer() filter.  This limitation may be lifted in
   *     the future.  It is possible to have multiple ApplyLabelTransformer
   *     filters in a Union() filter, though in this case, each copy of a cell
   *     gets a different label.
   *
   * @param label the label applied to each cell.  The labels must be at most 15
   *     characters long, and must match the `[a-z0-9\\-]+` pattern.  The server
   *     validates the filter and will return a
   *     `grpc::StatusCode::INVALID_ARGUMENT` if the label does not meet these
   *     requirements. This function makes no attempt to validate the @p label
   *     parameter before sending it to the server.
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
  /**
   * Returns a per-row conditional filter expression.
   *
   * For each row the @p predicate filter is evaluated, if it returns any
   * cells, then the cells returned by @p true_filter are returned, otherwise
   * the cells from @p false_filter are returned.
   *
   * The server validates the tree of filters, and rejects them if any contain
   * invalid values. The server may impose additional restrictions on the
   * resulting collection of filters.  This function makes no attempt to
   * validate the input before sending it to the server.
   */
  static Filter Condition(Filter predicate, Filter true_filter,
                          Filter false_filter) {
    Filter tmp;
    auto& condition = *tmp.filter_.mutable_condition();
    condition.mutable_predicate_filter()->Swap(&predicate.filter_);
    condition.mutable_true_filter()->Swap(&true_filter.filter_);
    condition.mutable_false_filter()->Swap(&false_filter.filter_);
    return tmp;
  }

  /**
   * Return a chain filter.
   *
   * The filter returned by this function acts like a pipeline.  The output
   * row from each stage is passed on as input for the next stage.
   *
   * @tparam FilterTypes the type of the filter arguments.  They must all be
   *    convertible to Filter.
   * @param stages the filter stages.  The filter must contain at least two
   *    stages. The server validates each stage, and will reject them as
   *    described in their corresponding function. The server may also impose
   *    additional restrictions on the composition of the Chain. This function
   *    makes no attempt at validating the stages locally, the Chain filter is
   *    sent as-is it to the server.
   */
  template <typename... FilterTypes>
  static Filter Chain(FilterTypes&&... stages) {
    // This ugly thing provides a better compile-time error message than
    // just letting the compiler figure things out 3 levels deep
    // as it recurses on append_types().
    static_assert(
        internal::conjunction<
            std::is_convertible<FilterTypes, Filter>...>::value,
        "The arguments passed to Chain(...) must be convertible to Filter");
    Filter tmp;
    auto& chain = *tmp.filter_.mutable_chain();
    std::initializer_list<Filter> list{std::forward<FilterTypes>(stages)...};
    for (Filter const& f : list) {
      *chain.add_filters() = f.as_proto();
    }
    return tmp;
  }

  /**
   * Return a filter that interleaves the results of many other filters.
   *
   * This filter executes each stream in parallel and then merges the results by
   * interleaving the output from each stream.  The
   * [proto
   * file](https://github.com/googleapis/googleapis/blob/master/google/bigtable/v2/data.proto)
   * has a nice illustration in the documentation of
   * `google.bigtable.v2.RowFilter.Interleave`.
   *
   * In brief, if the input cells are c1, c2, c3, ..., and you have three
   * subfilters S1, S2, and S3, the output of Interleave(S1, S2, S3) is:
   * S1(c1), S2(c1), S3(c1), S1(d2), S2(d2), S3(c2), S1(c3), S2(c3), S3(c2), ...
   * where some of the Si(c_j) values may be empty if the filter discards the
   * cell altogether.
   *
   * @tparam FilterTypes the type of the filter arguments.  They must all be
   *    convertible for Filter.
   * @param streams the filters to interleave. The filter must contain at least
   *    two streams. The server validates each stream, and will reject them as
   *    described in their corresponding function. The server may also impose
   *    additional restrictions on the overall composition of the Interleave
   *    filter. This function makes no attempt at validating the streams
   *    locally, the Interleave filter is sent as-is to the server.
   */
  template <typename... FilterTypes>
  static Filter Interleave(FilterTypes&&... streams) {
    static_assert(internal::conjunction<
                      std::is_convertible<FilterTypes, Filter>...>::value,
                  "The arguments passed to Interleave(...) must be convertible"
                  " to Filter");
    Filter tmp;
    auto& interleave = *tmp.filter_.mutable_interleave();
    std::initializer_list<Filter> list{std::forward<FilterTypes>(streams)...};
    for (Filter const& f : list) {
      *interleave.add_filters() = f.as_proto();
    }
    return tmp;
  }

  /**
   * Return a filter that outputs all cells ignoring intermediate filters.
   *
   * Please read the documentation in the
   * [proto
   * file](https://github.com/googleapis/googleapis/blob/master/google/bigtable/v2/data.proto)
   * for a detailed description.  In short, this is an advanced filter to
   * facilitate debugging.  You can explore the intermediate results of a
   * complex filter expression by injecting a filter of this type.
   */
  static Filter Sink() {
    Filter tmp;
    tmp.filter_.set_sink(true);
    return tmp;
  }
  //@}

  /// Return the filter expression as a protobuf.
  ::google::bigtable::v2::RowFilter const& as_proto() const& { return filter_; }

  /// Move out the underlying protobuf value.
  ::google::bigtable::v2::RowFilter&& as_proto() && {
    return std::move(filter_);
  }

 private:
  /// An empty filter, discards all data.
  Filter() : filter_() {}

 private:
  google::bigtable::v2::RowFilter filter_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_FILTERS_H_
