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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_KEYS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_KEYS_H

#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include <google/spanner/v1/keys.pb.h>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct KeySetInternals;
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * A `Key` is a collection of `Value` objects where the i'th value corresponds
 * to the i'th component of the table or primary index key.
 *
 * In C++, this is implemented as a `std::vector<Value>`. See the `MakeKey`
 * factory function below for an easy way to construct a valid `Key` instance.
 */
using Key = std::vector<Value>;

/**
 * Constructs a `Key` from the given arguments.
 *
 * @par Example
 * @snippet samples.cc make-key
 */
template <typename... Ts>
Key MakeKey(Ts&&... ts) {
  return Key{Value(std::forward<Ts>(ts))...};
}

/**
 * The `KeyBound` class is a regular type that represents an open or closed
 * endpoint for a range of keys.
 *
 * A range of keys is defined by a starting `KeyBound` and an ending
 * `KeyBound`, and it logically includes all intermediate keys, optionally
 * including/excluding the bounds.
 *
 * `KeyBound`s can be "open", meaning the matching row will be excluded from
 * the results, or "closed" meaning the matching row will be included.
 * `KeyBound` instances should be created with the `MakeKeyBoundOpen()` or
 * `MakeKeyBoundClosed()` factory functions.
 *
 * @par Example
 * @snippet samples.cc make-keybound-open
 *
 * @par Example
 * @snippet samples.cc make-keybound-closed
 */
class KeyBound {
 public:
  /// An enum indicating whether the `Key` is included (closed) or excluded
  /// (open).
  enum class Bound { kClosed, kOpen };

  /// Not default constructible
  KeyBound() = delete;

  /// Constructs an instance with the given @p key and @p bound.
  KeyBound(Key key, Bound bound) : key_(std::move(key)), bound_(bound) {}

  /// @name Copy and move semantics.
  ///@{
  KeyBound(KeyBound const&) = default;
  KeyBound& operator=(KeyBound const&) = default;
  KeyBound(KeyBound&&) = default;
  KeyBound& operator=(KeyBound&&) = default;
  ///@}

  /// Returns the `Key`.
  Key const& key() const& { return key_; }

  /// Returns the `Key` (by move).
  Key&& key() && { return std::move(key_); }

  /// Returns the `Bound`.
  Bound bound() const { return bound_; }

  /// @name Equality.
  ///@{
  friend bool operator==(KeyBound const& a, KeyBound const& b);
  friend bool operator!=(KeyBound const& a, KeyBound const& b) {
    return !(a == b);
  }
  ///@}

 private:
  Key key_;
  Bound bound_;
};

/**
 * Returns a "closed" `KeyBound` with a `Key` constructed from the given
 * arguments.
 *
 * @par Example
 * @snippet samples.cc make-keybound-closed
 */
template <typename... Ts>
KeyBound MakeKeyBoundClosed(Ts&&... ts) {
  return KeyBound(MakeKey(std::forward<Ts>(ts)...), KeyBound::Bound::kClosed);
}

/**
 * Returns an "open" `KeyBound` with a `Key` constructed from the given
 * arguments.
 *
 * @par Example
 * @snippet samples.cc make-keybound-open
 */
template <typename... Ts>
KeyBound MakeKeyBoundOpen(Ts&&... ts) {
  return KeyBound(MakeKey(std::forward<Ts>(ts)...), KeyBound::Bound::kOpen);
}

/**
 * The `KeySet` class is a regular type that represents a collection of `Key`s.
 *
 * Users can construct a `KeySet` instance, then add `Key`s and ranges of
 * `Key`s to the set. The caller is responsible for ensuring that all keys in a
 * given `KeySet` instance contain the same number and types of values.
 *
 * Users may also optionally construct an instance that
 * represents all keys with `KeySet::All()`.
 *
 * @par Example
 * @snippet samples.cc keyset-add-key
 *
 * @par Example
 * @snippet samples.cc keyset-all
 *
 */
class KeySet {
 public:
  /**
   * Returns a `KeySet` that represents the set of "All" keys for the index.
   *
   * @par Example
   * @snippet samples.cc keyset-all
   */
  static KeySet All() {
    KeySet ks;
    ks.proto_.set_all(true);
    return ks;
  }

  /// Constructs an empty `KeySet`.
  KeySet() = default;

  // Copy and move constructors and assignment operators.
  KeySet(KeySet const&) = default;
  KeySet& operator=(KeySet const&) = default;
  KeySet(KeySet&&) = default;
  KeySet& operator=(KeySet&&) = default;

  /**
   * Adds the given @p key to the `KeySet`.
   *
   * @par Example
   * @snippet samples.cc keyset-add-key
   */
  KeySet& AddKey(Key key);

  /**
   * Adds a range of keys defined by the given `KeyBound`s.
   *
   * @par Example
   * @snippet samples.cc keyset-add-key
   */
  KeySet& AddRange(KeyBound start, KeyBound end);

  /// @name Equality
  ///@{
  friend bool operator==(KeySet const& a, KeySet const& b);
  friend bool operator!=(KeySet const& a, KeySet const& b) { return !(a == b); }
  ///@}

 private:
  friend struct spanner_internal::SPANNER_CLIENT_NS::KeySetInternals;
  explicit KeySet(google::spanner::v1::KeySet proto)
      : proto_(std::move(proto)) {}

  google::spanner::v1::KeySet proto_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct KeySetInternals {
  static ::google::spanner::v1::KeySet ToProto(spanner::KeySet&& ks) {
    return std::move(ks.proto_);
  }

  static spanner::KeySet FromProto(::google::spanner::v1::KeySet&& proto) {
    return spanner::KeySet(std::move(proto));
  }
};

inline ::google::spanner::v1::KeySet ToProto(spanner::KeySet ks) {
  return KeySetInternals::ToProto(std::move(ks));
}

inline spanner::KeySet FromProto(::google::spanner::v1::KeySet ks) {
  return KeySetInternals::FromProto(std::move(ks));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_KEYS_H
