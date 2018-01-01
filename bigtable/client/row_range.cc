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

#include "bigtable/client/row_range.h"

#include <iostream>
#include <thread>

#include <absl/memory/memory.h>
#include <absl/strings/str_cat.h>

namespace btproto = ::google::bigtable::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
const std::string RowRange::SAMPLE_KEY = "";
;

/**
 * The contructor for RowRange: [begin, end).
 * In the constructor and all the builder methods, We don't check
 * if begin <= end. This anomaly is caught during range manipulation
 * (i.e., intersect, contains)
 */
RowRange::RowRange(std::string begin, std::string end) {
  range_.set_start_key_closed(std::move(begin));
  if (!end.empty()) range_.set_end_key_open(std::move(end));
}

/**
 * An empty range satisfies the following conditions:
 *   - Any semi-open or open range [x, x), (x,x], (x,x) where x could be empty.
 *   - 'end' is set (otherwise it would be an infinite range).
 *   - Not a closed interval [x,x] with non-empty x.
 *
 * We use the right-open [x,x) as the default empty range.
 * For convenience and the readability of unit tests,
 * we set the SAMPLE_KEY = "".
 * A set end_key (even to "") makes a finite range.
 * (Only this method is allowed set end_key to "". A user can't create a
 * finite range with end_key "", which is reserved for (semi-)infinite range).
 */
RowRange RowRange::empty() {
  RowRange tmp;
  tmp.range_.set_start_key_closed(RowRange::SAMPLE_KEY);
  // Only this method can set end_key as "".
  tmp.range_.set_end_key_open(RowRange::SAMPLE_KEY);
  return tmp;
}

/**
 * Infinite range has the following properties:
 *   - 'end' value is not set.
 *   - 'start' could be  don't cared.
 *      The proto also has the property that, an unset start key will have
 *      value "", inclusive.
 *  Gives the range ["..NOT_SET).
 *
 *  Note: We support right-sided (or semi) inifinity, where start key can be set
 * to any value: [x..NOT_SET); an empty end key implies semi-infinite range.
 */
RowRange RowRange::infinite_range() { return RowRange(); }

/**
 * Default one, right-open: [begin, end).
 * Both start and end should be set.
 */
RowRange RowRange::right_open(std::string begin, std::string end) {
  RowRange tmp;
  tmp.range_.set_start_key_closed(std::move(begin));
  if (!end.empty())  // (Semi-)infinite range: end = "".
    tmp.range_.set_end_key_open(std::move(end));
  return tmp;
}

/**
 * open interval: (begin, end), both endpoints should be set.
 */
RowRange RowRange::open_interval(std::string begin, std::string end) {
  RowRange tmp;
  tmp.range_.set_start_key_open(std::move(begin));
  if (!end.empty())  // (Semi-)infinite range: end = "".
    tmp.range_.set_end_key_open(std::move(end));
  return tmp;
}

// closed interval: [begin, end], both endpoints should be set.
RowRange RowRange::closed_interval(std::string begin, std::string end) {
  RowRange tmp;
  tmp.range_.set_start_key_closed(std::move(begin));
  if (!end.empty())  // (Semi-)infinite range: end = "".
    tmp.range_.set_end_key_closed(std::move(end));
  return tmp;
}

// left-open: (begin, end], both endpoints should be set.
RowRange RowRange::left_open(std::string begin, std::string end) {
  RowRange tmp;
  tmp.range_.set_start_key_open(std::move(begin));
  if (!end.empty())  // (Semi-)infinite range: end = "".
    tmp.range_.set_end_key_closed(std::move(end));
  return tmp;
}

/**
 * Given any key 'prefix', the prefix range is given as the right-open interval:
 *   ['prefix', successor('prefix')).
 *
 * When prefix is of the form "\xFF\xFF\xFF...\xFF", the successor is
 * "" (empty string). In such a case, we set end key to 'prefix';
 *
 */
RowRange RowRange::prefix_range(std::string prefix) {
  RowRange tmp;
  std::string next_prefix = Successor(prefix);

  if (next_prefix.empty()) next_prefix = prefix;
  tmp.range_.set_start_key_closed(std::move(prefix));
  if (!next_prefix.empty())  // Infinite range for prefix="".
    tmp.range_.set_end_key_open(std::move(next_prefix));
  return tmp;
}

// An alias for right_open().
RowRange RowRange::range(std::string begin, std::string end) {
  return right_open(begin, end);
}

/*
 * Increment the given key: gives the permutation string (in the key-space)
 * that immediately follows the 'key'.
 * Please note that, for a key of the form "\xFF\xFF\xFF...\xFF", the successor
 * is "" (empty string).
 */
std::string RowRange::Successor(std::string const& key) {
  if (key.empty()) {
    return "";
  }
  std::size_t position = key.find_last_not_of('\xFF');

  if (position == std::string::npos) {
    return "";
  }
  const char last_char = key[position] + 1;
  return (key.substr(0, position) + last_char);
}

/**
 * A range  can be of folling types:
 *  - start is NOT_SET or set to "",  and end is NOT_SET: infinite range
 *    spanning the complete domain of the row key.
 *  - start key set to a non-empty 'x', and end is NOT_SET: one-sided infinite
 *    range starting from x.
 */
bool RowRange::contains(std::string_view row) const {
  KeyType start_type{GetStartKeyType()}, end_type{GetEndKeyType()};
  std::string start_key{GetStartKey()}, end_key{GetEndKey()};
  bool result = true;

  if (row.empty()) return true;

  // Check for start key.
  switch (start_type) {
    case KeyType::OPEN:
      result = start_key < row;
      break;
    case KeyType::CLOSED:
      result = start_key <= row;
      break;
    case KeyType::NOT_SET:
      result = true;
  }

  // Check for end key.
  switch (end_type) {
    case KeyType::OPEN:
      result = result && (row < end_key);
      break;
    case KeyType::CLOSED:
      result = result && (row <= end_key);
      break;
    case KeyType::NOT_SET:
      break;
  }

  return result;
}

/*
 * Two ranges are equal, if:
 *  - both are empty.
 *  - start keys and types are same; in case the key value is empty,
 *    igore the equality condition on their types (This additional check is
 *    necessary for consistent range representation,
 *      infinite_range() == range("", "").
 *  - 'end' values and their types are the same.
 *
 * For simplicity, we omit the special boundary case:
 *   An inifinite range with start NOT_SET or set to "" (empty string)
 *   of type CLOSED. Both should be treated equal.
 */
bool RowRange::operator==(RowRange const& rhs) const {
  std::string start_key{GetStartKey()}, end_key{GetEndKey()},
      rhs_start_key{rhs.GetStartKey()}, rhs_end_key{rhs.GetEndKey()};
  KeyType start_type{GetStartKeyType()}, end_type{GetEndKeyType()},
      rhs_start_type{rhs.GetStartKeyType()}, rhs_end_type{rhs.GetEndKeyType()};

  return ((this->IsEmpty() && rhs.IsEmpty()) ||
          ((start_key == rhs_start_key &&
            /* If key is empty, type is 'don't care'*/
            (start_key.empty() || start_type == rhs_start_type)) &&
           (end_key == rhs_end_key && end_type == rhs_end_type)));
}

// The intersection of two ranges (useful for retries).
RowRange RowRange::intersect(RowRange const& lhs, RowRange const& rhs) {
  std::string lhs_start_key{lhs.GetStartKey()},
              lhs_end_key{lhs.GetEndKey()},
              rhs_start_key{rhs.GetStartKey()},
              rhs_end_key{rhs.GetEndKey()};
  KeyType lhs_start_type{lhs.GetStartKeyType()},
          lhs_end_type{lhs.GetEndKeyType()},
          rhs_start_type{rhs.GetStartKeyType()},
          rhs_end_type{rhs.GetEndKeyType()};
  std::string result_start_key{lhs_start_key}, result_end_key{lhs_end_key};
  KeyType result_start_type{lhs_start_type}, result_end_type{lhs_end_type};

  // Fix the start key and type: pick the max value.
  FixKeyPointAndTypeIntersect(PositionType::START, result_start_key,
                              result_start_type, rhs_start_key, rhs_start_type);
  // Fix end key and type: Pick the min value.
  FixKeyPointAndTypeIntersect(PositionType::END, result_end_key,
                              result_end_type, rhs_end_key, rhs_end_type);

  // Avoid making  result_end_key  smaller than result_start_key.
  if (result_end_key < result_start_key) {
    result_end_key = result_start_key;
  }

  RowRange tmp;
  SetProtoBufRangePoint(PositionType::START, tmp, result_start_key,
                        result_start_type);
  SetProtoBufRangePoint(PositionType::END, tmp, result_end_key,
                        result_end_type);
  return tmp;
}

/**
 * Two scenarios for empty range:
 * 1. For normal input values (of start and end), an empty range satisfies all
 * the properties below:
 *   - either start or end key point is open,
 *   - start key == end key,
 *   - end key is 'set' (i.e., a finite range).
 * 2. For anomalous inputs ('end' gets a value lower than that of 'start'),
 * a range is empty if:
 *    - the end is set and end < start.
 *
 *  We exclude open ranges like, for example, (x, Successor(x)) from
 *  consideration, as they could be prefix ranges with x being the prefix
 *  of a row key.
 */
bool RowRange::IsEmpty() const {
  KeyType start_type{GetStartKeyType()}, end_type{GetEndKeyType()};
  std::string start_key{GetStartKey()}, end_key{GetEndKey()};

  return (/* Case 1. */
          ((start_type == KeyType::OPEN || end_type == KeyType::OPEN) &&
           (start_key >= end_key) && (end_type != KeyType::NOT_SET)) ||
          /* Case 2: Anomalous inputs */
          (end_type != KeyType::NOT_SET && end_key < start_key));
}

/**
 * Get the type of the START of the range.
 * To avoid the conflict between two key types (StartKeyCase and EndKeyCase),
 * we don't overload the 'switch' statement, and provide two methods for
 * start and end keys
 */
RowRange::KeyType RowRange::GetStartKeyType() const {
  KeyType type = KeyType::CLOSED;

  auto key_type = range_.start_key_case();
  switch (key_type) {
    case btproto::RowRange::kStartKeyOpen:
      type = KeyType::OPEN;
      break;
    case btproto::RowRange::kStartKeyClosed:
    // Closed for inifinite ranges.
    case btproto::RowRange::START_KEY_NOT_SET:
      type = KeyType::CLOSED;
      break;
  }
  return type;
}

/**
 * Get the type (open, closed or not set) for the end key.
 */
RowRange::KeyType RowRange::GetEndKeyType() const {
  KeyType type = KeyType::NOT_SET;

  auto key_type = range_.end_key_case();
  switch (key_type) {
    case btproto::RowRange::kEndKeyOpen:
      type = KeyType::OPEN;
      break;
    case btproto::RowRange::kEndKeyClosed:
      type = KeyType::CLOSED;
      break;
    case btproto::RowRange::END_KEY_NOT_SET:
      type = KeyType::NOT_SET;
      break;
  }
  return type;
}

/**
 * Get the start key: if neither field is set, interpreted as
 * empty key, inclusive.
 */
std::string RowRange::GetStartKey() const {
  std::string result = "";

  switch (range_.start_key_case()) {
    case btproto::RowRange::kStartKeyOpen:
      result = range_.start_key_open();
      break;
    case btproto::RowRange::kStartKeyClosed:
      result = range_.start_key_closed();
      break;
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
  }
  return result;
}

/**
 * Get the end key: if neither filed is set interpreted as
 * infinite key, exclusive.
 */
std::string RowRange::GetEndKey() const {
  std::string result = "";

  switch (range_.end_key_case()) {
    case btproto::RowRange::kEndKeyOpen:
      result = range_.end_key_open();
      break;
    case btproto::RowRange::kEndKeyClosed:
      result = range_.end_key_closed();
      break;
    // Same as btproto::RowRange::START_KEY_NOT_SET.
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
  }
  return result;
}

/**
 * Finalize the end points for intersection.
 * position : gives the point position (start or end).
 */
void RowRange::FixKeyPointAndTypeIntersect(RowRange::PositionType position,
                                           std::string& result_key,
                                           RowRange::KeyType& result_type,
                                           std::string rhs_key,
                                           RowRange::KeyType rhs_type) {
  // Pick the max value for start point, and min value for the end.
  bool comparison = (position == PositionType::START ? result_key < rhs_key
                                                     : result_key > rhs_key);
  if (comparison) {
    result_key = rhs_key;
    result_type = rhs_type;
  } else if (result_key == rhs_key && KeyType::OPEN == rhs_type) {
    // prefer OPEN type in case of a tie.
    result_type = rhs_type;
  }
}

/**
 * Initialize the protocol Buffer within the RowRange.
 */
void RowRange::SetProtoBufRangePoint(RowRange::PositionType position,
                                     RowRange& row_range, std::string point_key,
                                     RowRange::KeyType point_type) {
  if (position == PositionType::START) {
    if (point_type == KeyType::OPEN)
      row_range.range_.set_start_key_open(point_key);
    else if (point_type == KeyType::CLOSED)
      row_range.range_.set_start_key_closed(point_key);
  } else {  // End point.
    if (point_type == KeyType::OPEN)
      row_range.range_.set_end_key_open(point_key);
    else if (point_type == KeyType::CLOSED)
      row_range.range_.set_end_key_closed(point_key);
  }
}

std::string RowRange::debug_string() const {
  KeyType start_type{GetStartKeyType()}, end_type{GetEndKeyType()};

  std::string result = "";
  switch (start_type) {
    case KeyType::OPEN:
      result = absl::StrCat("(", range_.start_key_open());
      break;
    case KeyType::CLOSED:
      result = absl::StrCat("[", range_.start_key_closed());
      break;
    case KeyType::NOT_SET:
      // For an unset start key, the value is "", inclusive.
      result = "[";
      break;
  }
  result = absl::StrCat(result, "..");
  switch (end_type) {
    case KeyType::OPEN:
      result = absl::StrCat(result, range_.end_key_open(), ")");
      break;
    case KeyType::CLOSED:
      result = absl::StrCat(result, range_.end_key_closed(), "]");
      break;
    case KeyType::NOT_SET:
      result = absl::StrCat(result + "NOT_SET)");
      break;
  }
  return result;
}

std::ostream& operator<<(std::ostream& out, RowRange const& rhs) {
  return out << rhs.debug_string();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
