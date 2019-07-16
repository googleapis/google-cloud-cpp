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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MUTATIONS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MUTATIONS_H_

#include "google/cloud/spanner/internal/tuple_utils.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include <google/spanner/v1/mutation.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
template <typename Op>
class WriteMutationBuilder;
}  // namespace internal

/**
 * A wrapper for Cloud Spanner mutations.
 *
 * In addition to the Data Manipulation Language (DML) based APIs, Cloud Spanner
 * supports the mutation API, where the application describes data modification
 * using a data structure instead of a SQL statement.
 *
 * This class serves as a wrapper for all mutations types. Use the builders,
 * such as `InsertMutationBuilder` or `UpdateMutationBuilder` to create objects
 * of this class.
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
class Mutation {
 public:
  /**
   * Creates an empty mutation.
   *
   * @note Empty mutations are not usable with the Cloud Spanner mutation API.
   */
  Mutation() = default;

  Mutation(Mutation&&) noexcept = default;
  Mutation& operator=(Mutation&&) noexcept = default;
  Mutation(Mutation const&) = default;
  Mutation& operator=(Mutation const&) = default;

  friend bool operator==(Mutation const& lhs, Mutation const& rhs);
  friend bool operator!=(Mutation const& lhs, Mutation const& rhs) {
    return !(lhs == rhs);
  }

  /// Convert the mutation to the underlying proto.
  google::spanner::v1::Mutation as_proto() && { return std::move(m_); }

  /**
   * Allows Google Test to print internal debugging information when test
   * assertions fail.
   *
   * @warning This is intended for debugging and human consumption only, not
   *   machine consumption as the output format may change without notice.
   */
  friend void PrintTo(Mutation const& m, std::ostream* os);

 private:
  template <typename Op>
  friend class internal::WriteMutationBuilder;
  explicit Mutation(google::spanner::v1::Mutation m) : m_(std::move(m)) {}

  google::spanner::v1::Mutation m_;
};

// This namespace contains implementation details. It is not part of the public
// API, and subject to change without notice.
namespace internal {
inline void PopulateListValue(google::protobuf::ListValue&) {}

/**
 * Initialize a `google::protobuf::ListValue` from a variadic list of C++ types.
 *
 * This function applies the conversions defined by #Value
 */
template <typename T, typename... Ts>
void PopulateListValue(google::protobuf::ListValue& lv, T&& head,
                       Ts&&... tail) {
  google::spanner::v1::Type type;
  google::protobuf::Value value;
  std::tie(type, value) = internal::ToProto(Value(std::forward<T>(head)));
  *lv.add_values() = std::move(value);
  PopulateListValue(lv, std::forward<Ts>(tail)...);
}

template <typename Op>
class WriteMutationBuilder {
 public:
  WriteMutationBuilder() { Op::mutable_field(m_); }

  explicit WriteMutationBuilder(std::vector<std::string> column_names) {
    auto& field = Op::mutable_field(m_);
    field.mutable_columns()->Reserve(column_names.size());
    for (auto& name : column_names) {
      *field.add_columns() = std::move(name);
    }
  }

  Mutation Build() const& { return Mutation(m_); }
  Mutation Build() && { return Mutation(std::move(m_)); }

  template <typename... Ts>
  WriteMutationBuilder& AddRow(Ts&&... values) {
    google::protobuf::ListValue lv;
    internal::PopulateListValue(lv, std::forward<Ts>(values)...);
    *Op::mutable_field(m_).add_values() = std::move(lv);
    return *this;
  }
  // TODO(#222) - consider an overload on `Row<Ts>`.

 private:
  google::spanner::v1::Mutation m_;
};

struct InsertOp {
  static google::spanner::v1::Mutation::Write& mutable_field(
      google::spanner::v1::Mutation& m) {
    return *m.mutable_insert();
  }
};

struct UpdateOp {
  static google::spanner::v1::Mutation::Write& mutable_field(
      google::spanner::v1::Mutation& m) {
    return *m.mutable_update();
  }
};

// TODO(#198) - use them in the (future) InsertOrUpdateMutationBuilder.
struct InsertOrUpdateOp {
  static google::spanner::v1::Mutation::Write& mutable_field(
      google::spanner::v1::Mutation& m) {
    return *m.mutable_insert_or_update();
  }
};

// TODO(#198) - use them in the (future) ReplaceMutationBuilder.
struct ReplaceOp {
  static google::spanner::v1::Mutation::Write& mutable_field(
      google::spanner::v1::Mutation& m) {
    return *m.mutable_replace();
  }
};

}  // namespace internal

/**
 * A helper class to construct "insert" mutations.
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
using InsertMutationBuilder =
    internal::WriteMutationBuilder<internal::InsertOp>;

/// Creates a simple insert mutation for the values in @p row.
template <typename... Ts>
Mutation MakeInsertMutation(Ts&&... values) {
  return InsertMutationBuilder().AddRow(std::forward<Ts>(values)...).Build();
}

/**
 * A helper class to construct "update" mutations.
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
using UpdateMutationBuilder =
    internal::WriteMutationBuilder<internal::UpdateOp>;

/// Creates a simple insert mutation for the values in @p row.
template <typename... Ts>
Mutation MakeUpdateMutation(Ts&&... values) {
  return UpdateMutationBuilder().AddRow(std::forward<Ts>(values)...).Build();
}

// TODO(#198 & #202) - Implement DeleteMutationBuilder.

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MUTATIONS_H_
