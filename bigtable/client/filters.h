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

#include <bigtable/client/version.h>
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
 *   which may be present in a binary value.
 */
class Filter {
 public:
  /// An empty filter, discards all data.
  Filter() : filter_() {}

  // TODO() - replace with = default if protobuf gets move constructors.
  Filter(Filter&& rhs) noexcept : Filter() { filter_.Swap(&rhs.filter_); }

  // TODO() - replace with = default if protobuf gets move constructors.
  Filter& operator=(Filter&& rhs) noexcept {
    Filter tmp(std::move(rhs));
    tmp.filter_.Swap(&filter_);
    return *this;
  }

  Filter(Filter const& rhs) = default;
  Filter& operator=(Filter const& rhs) = default;

  /// Create a filter that accepts only the last @a n values.
  static Filter Latest(int n) {
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
  static Filter Family(std::string pattern) {
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
  static Filter Column(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_column_qualifier_regex_filter(std::move(pattern));
    return tmp;
  }

  /**
   * Create a filter that accepts columns in the given range.
   *
   * Notice that the range is right-open, i.e., it represents [start,end)
   */
  static Filter ColumnRange(std::string begin, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_column_range_filter();
    range.set_start_qualifier_closed(std::move(begin));
    range.set_end_qualifier_open(std::move(end));
    return tmp;
  }

  /**
   * Return a filter that accepts cells in the given timestamp range.
   *
   * The range is right-open, i.e., it represents [start,end)
   */
  static Filter TimestampRangeMicros(std::int64_t start, std::int64_t end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_timestamp_range_filter();
    range.set_start_timestamp_micros(start);
    range.set_end_timestamp_micros(end);
    return tmp;
  }

  /**
   * Return a filter that accepts cells in the give timestamp range.
   *
   * The range is right-open, i.e., it represents [start,end)
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
  static Filter MatchingRowKeys(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_row_key_regex_filter(std::move(pattern));
    return tmp;
  }

  /**
   * Return a filter that matches keys matching the given regexp.
   *
   * @param pattern the regular expression.  It must be a valid
   *     [RE2](https://github.com/google/re2/wiki/Syntax) pattern.
   */
  static Filter MatchingValue(std::string pattern) { return Filter(); }

  /**
   * Return a filter matching a right-open interval of values.
   *
   * This filter matches all the values in the [@p begin,@p end) range.
   */
  static Filter ValueRange(std::string begin, std::string end) {
    Filter tmp;
    auto& range = *tmp.filter_.mutable_value_range_filter();
    range.set_start_value_closed(std::move(begin));
    range.set_end_value_open(std::move(end));
    return tmp;
  }

  static Filter ValueRangeLeftOpen() { return Filter(); }
  static Filter ValueRangeRightOpen() { return Filter(); }
  static Filter ValueRangeClosed() { return Filter(); }
  static Filter ValueRangeOpen() { return Filter(); }
  static Filter RowKeyRangeLeftOpen() { return Filter(); }
  static Filter RowKeyRangeRightOpen() { return Filter(); }
  static Filter RowKeyRangeClosed() { return Filter(); }
  static Filter RowKeyRangeOpen() { return Filter(); }
  static Filter RowKeyPrefix() { return Filter(); }

  static Filter SinkFilter() { return Filter(); }
  static Filter PassAllFilter() { return Filter(); }
  static Filter BlockAllFilter() { return Filter(); }

  static Filter RowSample(float lambda) { return Filter(); }
  static Filter Earliest(int n) { return Filter(); }
  static Filter ColumnLimit(int n) { return Filter(); }

  static Filter ApplyLabelTransformer() { return Filter(); }
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


  //@{
  /**
   * @name Composed filters.
   *
   * These filters compose several filters to build complex filter expressions.
   */
  /// Return a filter that selects between two other filters based on a
  /// predicate.
  static Filter Condition(Filter predicate, Filter true_filter,
                          Filter false_filter) {
    return Filter();
  }

  // TODO(coryan) - document ugly std::enable_if<> hack to ensure they all
  // are of type Filter.
  /**
   * Return a filter that only accepts the results of a pipeline of filters.
   * @tparam FilterTypes
   * @param a
   * @return
   */
  template <typename... FilterTypes>
  static Filter Chain(FilterTypes&&... a) {
    return Filter();
  }

  // TODO(coryan) - same ugly hack documentation needed ...
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
  google::bigtable::v2::RowFilter filter_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_
