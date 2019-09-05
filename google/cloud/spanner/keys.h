// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_

#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/keys.pb.h>
#include <string>
#include <type_traits>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class KeySet;
template <typename RowType>
class KeySetBuilder;

namespace internal {
::google::spanner::v1::KeySet ToProto(KeySet keyset);
KeySet FromProto(::google::spanner::v1::KeySet keyset);

template <typename T>
struct IsRow : std::false_type {};

template <typename... Ts>
struct IsRow<Row<Ts...>> : std::true_type {};
}  // namespace internal

/**
 * The `KeyBound` class is a regular type that represents one endpoint of an
 * interval of keys.
 *
 * `KeyBound`s can be "open", meaning the matching row will be excluded from
 * the results, or "closed" meaning the matching row will be included.
 * `KeyBound` instances should be created with the `MakeKeyBoundOpen()` or
 * `MakeKeyBoundClosed()` factory functions.
 *
 * @tparam KeyType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 */
template <typename RowType>
class KeyBound {
 public:
  static_assert(internal::IsRow<RowType>::value,
                "KeyType must be of type spanner::Row<>.");

  // Not default constructible
  KeyBound() = delete;

  // Copy and move constructors and assignment operators.
  KeyBound(KeyBound const& key_range) = default;
  KeyBound& operator=(KeyBound const& rhs) = default;
  KeyBound(KeyBound&& key_range) = default;
  KeyBound& operator=(KeyBound&& rhs) = default;

  RowType const& key() const& { return key_; }
  RowType&& key() && { return std::move(key_); }

  bool IsClosed() const { return mode_ == Mode::kClosed; }
  bool IsOpen() const { return mode_ == Mode::kOpen; }

  friend bool operator==(KeyBound const& lhs, KeyBound const& rhs) {
    return lhs.key_ == rhs.key_ && lhs.mode_ == rhs.mode_;
  }
  friend bool operator!=(KeyBound const& lhs, KeyBound const& rhs) {
    return !(lhs == rhs);
  }

 private:
  enum class Mode { kClosed, kOpen };

  KeyBound(RowType key, Mode mode) : key_(std::move(key)), mode_(mode) {}

  template <typename T>
  friend KeyBound<T> MakeKeyBoundClosed(T key);

  template <typename T>
  friend KeyBound<T> MakeKeyBoundOpen(T key);

  RowType key_;
  Mode mode_;
};

/**
 * Helper function to create a closed `KeyBound` on the key `spanner::Row`
 * provided.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param key spanner::Row<Types...>
 * @return KeyBound<RowType>
 */
template <typename RowType>
KeyBound<RowType> MakeKeyBoundClosed(RowType key) {
  return KeyBound<RowType>(std::move(key), KeyBound<RowType>::Mode::kClosed);
}

/**
 * Helper function to create an open `KeyBound` on the key `spanner::Row`
 * provided.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param key spanner::Row<Types...>
 * @return KeyBound<RowType>
 */
template <typename RowType>
KeyBound<RowType> MakeKeyBoundOpen(RowType key) {
  return KeyBound<RowType>(std::move(key), KeyBound<RowType>::Mode::kOpen);
}

/**
 * The `KeyRange` class is a regular type that represents the pair of
 * `KeyBound`s necessary to uniquely identify a contiguous group of key
 * `spanner::Row`s in its index.
 *
 * @tparam KeyType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 */
template <typename RowType>
class KeyRange {
 public:
  static_assert(internal::IsRow<RowType>::value,
                "KeyType must be of type spanner::Row<>.");

  // Not default constructible
  KeyRange() = delete;

  /**
   * Constructs a `KeyRange` with the given `KeyBound`s.
   */
  explicit KeyRange(KeyBound<RowType> start, KeyBound<RowType> end)
      : start_(std::move(start)), end_(std::move(end)) {}

  // Copy and move constructors and assignment operators.
  KeyRange(KeyRange const& key_range) = default;
  KeyRange& operator=(KeyRange const& rhs) = default;
  KeyRange(KeyRange&& key_range) = default;
  KeyRange& operator=(KeyRange&& rhs) = default;

  KeyBound<RowType> const& start() const& { return start_; }
  KeyBound<RowType>&& start() && { return std::move(start_); }

  KeyBound<RowType> const& end() const& { return end_; }
  KeyBound<RowType>&& end() && { return std::move(end_); }

  friend bool operator==(KeyRange const& lhs, KeyRange const& rhs) {
    return lhs.start_ == rhs.start_ && lhs.end_ == rhs.end_;
  }
  friend bool operator!=(KeyRange const& lhs, KeyRange const& rhs) {
    return !(lhs == rhs);
  }

 private:
  KeyBound<RowType> start_;
  KeyBound<RowType> end_;
};

/**
 * Helper function to create a `KeyRange` between two keys `spanner::Row`s with
 * both `KeyBound`s closed.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param start spanner::Row<Types...> of the starting key.
 * @param end spanner::Row<Types...> of the ending key.
 * @return KeyRange<RowType>
 */
template <typename RowType>
KeyRange<RowType> MakeKeyRangeClosed(RowType start, RowType end) {
  return MakeKeyRange(MakeKeyBoundClosed(std::move(start)),
                      MakeKeyBoundClosed(std::move(end)));
}

/**
 * Helper function to create a `KeyRange` between the `KeyBound`s provided.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param start spanner::Row<Types...> of the starting key.
 * @param end spanner::Row<Types...> of the ending key.
 * @return KeyRange<RowType>
 */
template <typename RowType>
KeyRange<RowType> MakeKeyRange(KeyBound<RowType> start, KeyBound<RowType> end) {
  return KeyRange<RowType>(std::move(start), std::move(end));
}

/**
 * The `KeySet` class is a regular type that represents the collection of
 * `spanner::Row`s necessary to uniquely identify a arbitrary group of rows in
 * its index, or all the keys in an index.
 *
 * Unlike `KeySetBuilder`, `KeySet` stores its keys and key ranges in a type
 * erased fashion.
 *
 */
class KeySet {
 public:
  /**
   * Returns a `KeySet` that represents the set of "All" keys for the index.
   */
  static KeySet All() {
    KeySet ks;
    ks.proto_.set_all(true);
    return ks;
  }

  /**
   * Constructs an empty `KeySet`.
   */
  KeySet() = default;

  // Copy and move constructors and assignment operators.
  KeySet(KeySet const& key_range) = default;
  KeySet& operator=(KeySet const& rhs) = default;
  KeySet(KeySet&& key_range) = default;
  KeySet& operator=(KeySet&& rhs) = default;

  /// @name Equality
  /// Order of keys and key ranges in the `KeySet` is considered.
  ///@{
  friend bool operator==(KeySet const& lhs, KeySet const& rhs);
  friend bool operator!=(KeySet const& lhs, KeySet const& rhs);
  ///@}

 private:
  explicit KeySet(google::spanner::v1::KeySet key_set)
      : proto_(std::move(key_set)) {}

  friend ::google::spanner::v1::KeySet internal::ToProto(KeySet keyset);
  friend KeySet internal::FromProto(::google::spanner::v1::KeySet keyset);

  google::spanner::v1::KeySet proto_;
};

/**
 * The `KeySetBuilder` class is a regular type that represents the collection of
 * `spanner::Row`s necessary to uniquely identify a arbitrary group of rows in
 * its index.
 *
 * A `KeySetBuilder` can consist of multiple `KeyRange` and/or `spanner::Row`
 * instances.
 *
 * If the same key is specified multiple times in the `KeySetBuilder` (for
 * example if two `KeyRange`s, two keys, or a key and a `KeyRange` overlap),
 * Cloud Spanner behaves as if the key were only specified once.
 *
 * @tparam KeyType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 *
 * @par Example
 * @snippet samples.cc key-set-builder
 */
template <typename RowType>
class KeySetBuilder {
 public:
  static_assert(internal::IsRow<RowType>::value,
                "KeyType must be of type spanner::Row<>.");
  /**
   * Constructs an empty `KeySetBuilder`.
   */
  KeySetBuilder() = default;

  /// Copy and move constructors and assignment operators.
  KeySetBuilder(KeySetBuilder const& key_range) = default;
  KeySetBuilder& operator=(KeySetBuilder const& rhs) = default;
  KeySetBuilder(KeySetBuilder&& key_range) = default;
  KeySetBuilder& operator=(KeySetBuilder&& rhs) = default;

  /**
   * Constructs a `KeySetBuilder` with a single key `spanner::Row`.
   */
  explicit KeySetBuilder(RowType key) { keys_.push_back(std::move(key)); }

  /**
   * Constructs a `KeySetBuilder` with a single `KeyRange`.
   */
  explicit KeySetBuilder(KeyRange<RowType> key_range) {
    key_ranges_.push_back(std::move(key_range));
  }

  /**
   * Returns the key `spanner::Row`s in the `KeySetBuilder`.
   * These keys are separate from the collection of `KeyRange`s.
   */
  std::vector<RowType> const& keys() const { return keys_; }

  /**
   * Returns the `KeyRange`s in the `KeySetBuilder`.
   * These are separate from the collection of individual key `spanner::Row`s.
   */
  std::vector<KeyRange<RowType>> const& key_ranges() const {
    return key_ranges_;
  }

  /// Adds a key `spanner::Row` to the `KeySetBuilder`.
  KeySetBuilder&& Add(RowType key) && {
    keys_.push_back(std::move(key));
    return std::move(*this);
  }

  /// Adds a key `spanner::Row` to the `KeySetBuilder`.
  KeySetBuilder& Add(RowType key) & {
    keys_.push_back(std::move(key));
    return *this;
  }

  /// Adds a `KeyRange` to the `KeySetBuilder`.
  KeySetBuilder&& Add(KeyRange<RowType> key_range) && {
    key_ranges_.push_back(std::move(key_range));
    return std::move(*this);
  }

  /// Adds a `KeyRange` to the `KeySetBuilder`.
  KeySetBuilder& Add(KeyRange<RowType> key_range) & {
    key_ranges_.push_back(std::move(key_range));
    return *this;
  }

  /// Builds a type-erased `KeySet` from the contents of the `KeySetBuilder`.
  KeySet Build() && {
    google::spanner::v1::KeySet ks;
    for (auto& key : keys_) {
      Append(ks.add_keys(), std::move(key).values());
    }
    for (auto& range : key_ranges_) {
      auto* kr = ks.add_ranges();
      auto* start = range.start().IsClosed() ? kr->mutable_start_closed()
                                             : kr->mutable_start_open();
      Append(start, std::move(range).start().key().values());
      auto* end = range.end().IsClosed() ? kr->mutable_end_closed()
                                         : kr->mutable_end_open();
      Append(end, std::move(range).end().key().values());
    }
    return internal::FromProto(std::move(ks));
  }

  /// Builds a type-erased `KeySet` from the contents of the `KeySetBuilder`.
  KeySet Build() const& {
    auto copy = *this;
    return std::move(copy).Build();
  }

  // TODO(#322): Add methods to insert ranges of Keys and KeyRanges.
  // TODO(#323): Add methods to remove Keys or KeyRanges.

 private:
  template <std::size_t N>
  static void Append(google::protobuf::ListValue* lv,
                     std::array<Value, N>&& a) {
    for (auto& v : a) {
      *lv->add_values() = internal::ToProto(std::move(v)).second;
    }
  }

  std::vector<RowType> keys_;
  std::vector<KeyRange<RowType>> key_ranges_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_
