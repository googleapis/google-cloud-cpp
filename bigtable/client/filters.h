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

  /// Return a filter that matches column families matching the given regexp.
  static Filter Family(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_family_name_regex_filter(std::move(pattern));
    return tmp;
  }

  /// Create a filter that accepts only columns matching the given regexp.
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

  /// Return a filter that matches keys matching the given regexp.
  static Filter MatchingRowKeys(std::string pattern) {
    Filter tmp;
    tmp.filter_.set_row_key_regex_filter(std::move(pattern));
    return tmp;
  }

  static Filter StripValue() { return Filter(); }

  static Filter StripMatchingValues(std::string pattern) { return Filter(); }

  static Filter ValueRange(std::string begin, std::string end) {
    return Filter();
  }

  static Filter Condition(Filter predicate, Filter true_filter,
                          Filter false_filter) {
    return Filter();
  }

  /// Create a chain of filters
  // TODO(coryan) - document ugly std::enable_if<> hack to ensure they all
  // are of type Filter.
  template <typename... FilterTypes>
  static Filter Chain(FilterTypes&&... a) {
    return Filter();
  }

  /// Create a set of parallel interleaved filters
  // TODO(coryan) - same ugly hack documentation needed ...
  template <typename... FilterTypes>
  static Filter Interleave(FilterTypes&&... a) {
    return Filter();
  }

  google::bigtable::v2::RowFilter as_proto() const { return filter_; }

 private:
  google::bigtable::v2::RowFilter filter_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_FILTERS_H_
