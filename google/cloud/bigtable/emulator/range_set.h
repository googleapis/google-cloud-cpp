// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_RANGE_SET_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_RANGE_SET_H

#include "absl/types/variant.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <ostream>
#include <set>
#include <string>

namespace google {
namespace bigtable {
namespace v2 {
class RowRange;
class ValueRange;
class ColumnRange;
class TimestampRange;
}  // namespace v2
}  // namespace bigtable
namespace cloud {
namespace bigtable {
namespace emulator {

class StringRangeSet {
 public:
  class Range {
   public:
    struct Infinity {};
    using Value = absl::variant<Infinity, std::string>;

    Range(Value start, bool start_open, Value end, bool end_open);
    static StatusOr<Range> FromRowRange(
        google::bigtable::v2::RowRange const& row_range);
    static StatusOr<Range> FromValueRange(
        google::bigtable::v2::ValueRange const& value_range);
    static StatusOr<Range> FromColumnRange(
        google::bigtable::v2::ColumnRange const& column_range);

    Value const& start() const & { return start_; }
    std::string const& start_finite() const& {
      return absl::get<std::string>(start_);
    }
    Value&& start() && { return std::move(start_); }
    bool start_open() const { return start_open_; }
    bool start_closed() const { return !start_open_; }
    void set_start(Value start, bool start_open);

    Value const& end() const & { return end_; }
    Value&& end() && { return std::move(end_); }
    void set_end(Value end, bool end_open);
    bool end_open() const { return end_open_; }
    bool end_closed() const { return !end_open_; }

    bool IsBelowStart(Value const &value) const;
    bool IsAboveEnd(Value const &value) const;
    bool IsWithin(Value const &value) const;
    bool IsEmpty() const;

    static bool IsEmpty(StringRangeSet::Range::Value const& start,
                        bool start_open,
                        StringRangeSet::Range::Value const& end, bool end_open);

   private:
    Value start_;
    bool start_open_;
    Value end_;
    bool end_open_;
  };

  struct RangeValueLess {
    bool operator()(Range::Value const& lhs, Range::Value const& rhs) const;
  };

  struct RangeStartLess {
    bool operator()(Range const& lhs, Range const& rhs) const;
  };

  struct RangeEndLess {
    bool operator()(Range const& lhs, Range const& rhs) const;
  };

  static StringRangeSet All();
  static StringRangeSet Empty();
  void Insert(Range inserted_range);

  std::set<Range, RangeStartLess> const& disjoint_ranges() const {
    return disjoint_ranges_;
  };


 private:
  std::set<Range, RangeStartLess> disjoint_ranges_;
};

bool operator==(StringRangeSet::Range::Value const& lhs,
                StringRangeSet::Range::Value const& rhs);

std::ostream& operator<<(std::ostream& os,
                         StringRangeSet::Range::Value const& value);

bool operator==(StringRangeSet::Range const& lhs,
                StringRangeSet::Range const& rhs);

std::ostream& operator<<(std::ostream& os,
                         StringRangeSet::Range const& range);


class TimestampRangeSet {
 public:
  class Range {
   public:
    using Value = std::chrono::milliseconds;

    Range(Value start, Value end);
    static StatusOr<Range> FromTimestampRange(
        google::bigtable::v2::TimestampRange const& timestamp_range);

    Value start() const { return start_; }
    Value start_finite() const { return start_; }
    bool start_open() const { return false; }
    bool start_closed() const { return true; }
    void set_start(Value start) { start_ = start; }

    Value end() const { return end_; }
    bool end_open() const { return true; }
    bool end_closed() const { return false; }
    void set_end(Value end) { end_ = end; }

    bool IsBelowStart(Value value) const { return value < start_; }
    bool IsAboveEnd(Value value) const;
    bool IsWithin(Value value) const;

    static bool IsEmpty(TimestampRangeSet::Range::Value start,
                        TimestampRangeSet::Range::Value end);

   private:
    Value start_;
    Value end_;
  };

  struct RangeStartLess {
    bool operator()(Range const& lhs, Range const& rhs) const;
  };

  struct RangeEndLess {
    bool operator()(Range const& lhs, Range const& rhs) const;
  };

  static TimestampRangeSet All();
  static TimestampRangeSet Empty();
  void Insert(Range inserted_range);

  std::set<Range, RangeStartLess> const& disjoint_ranges() const {
    return disjoint_ranges_;
  };


 private:
  std::set<Range, RangeStartLess> disjoint_ranges_;
};

bool operator==(TimestampRangeSet::Range const& lhs,
                TimestampRangeSet::Range const& rhs);

std::ostream& operator<<(std::ostream& os,
                         TimestampRangeSet::Range const& range);

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_RANGE_SET_H
