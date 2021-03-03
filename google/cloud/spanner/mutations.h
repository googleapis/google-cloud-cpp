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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MUTATIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MUTATIONS_H

#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include <google/spanner/v1/mutation.pb.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
template <typename Op>
class WriteMutationBuilder;
class DeleteMutationBuilder;
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {

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

  Mutation(Mutation&&) = default;
  Mutation& operator=(Mutation&&) = default;
  Mutation(Mutation const&) = default;
  Mutation& operator=(Mutation const&) = default;

  friend bool operator==(Mutation const& lhs, Mutation const& rhs);
  friend bool operator!=(Mutation const& lhs, Mutation const& rhs) {
    return !(lhs == rhs);
  }

  /// Convert the mutation to the underlying proto.
  google::spanner::v1::Mutation as_proto() && { return std::move(m_); }
  google::spanner::v1::Mutation as_proto() const& { return m_; }

  /**
   * Allows Google Test to print internal debugging information when test
   * assertions fail.
   *
   * @warning This is intended for debugging and human consumption only, not
   *   machine consumption as the output format may change without notice.
   */
  friend void PrintTo(Mutation const& m, std::ostream* os);

 private:
  google::spanner::v1::Mutation& proto() & { return m_; }

  template <typename Op>
  friend class spanner_internal::SPANNER_CLIENT_NS::WriteMutationBuilder;
  friend class spanner_internal::SPANNER_CLIENT_NS::DeleteMutationBuilder;
  explicit Mutation(google::spanner::v1::Mutation m) : m_(std::move(m)) {}

  google::spanner::v1::Mutation m_;
};

/**
 * An ordered sequence of mutations to pass to `Client::Commit()` or return
 * from the `Client::Commit()` mutator.
 */
using Mutations = std::vector<Mutation>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

// This namespace contains implementation details. It is not part of the public
// API, and subject to change without notice.
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

template <typename Op>
class WriteMutationBuilder {
 public:
  WriteMutationBuilder(std::string table_name,
                       std::vector<std::string> column_names) {
    auto& field = Op::mutable_field(m_.proto());
    field.set_table(std::move(table_name));
    field.mutable_columns()->Reserve(static_cast<int>(column_names.size()));
    for (auto& name : column_names) {
      field.add_columns(std::move(name));
    }
  }

  spanner::Mutation Build() const& { return m_; }
  spanner::Mutation&& Build() && { return std::move(m_); }

  WriteMutationBuilder& AddRow(std::vector<spanner::Value> values) & {
    auto& lv = *Op::mutable_field(m_.proto()).add_values();
    for (auto& v : values) {
      std::tie(std::ignore, *lv.add_values()) =
          spanner_internal::ToProto(std::move(v));
    }
    return *this;
  }

  WriteMutationBuilder&& AddRow(std::vector<spanner::Value> values) && {
    return std::move(AddRow(std::move(values)));
  }

  template <typename... Ts>
  WriteMutationBuilder& EmplaceRow(Ts&&... values) & {
    return AddRow({spanner::Value(std::forward<Ts>(values))...});
  }

  template <typename... Ts>
  WriteMutationBuilder&& EmplaceRow(Ts&&... values) && {
    return std::move(EmplaceRow(std::forward<Ts>(values)...));
  }

 private:
  spanner::Mutation m_;
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

struct InsertOrUpdateOp {
  static google::spanner::v1::Mutation::Write& mutable_field(
      google::spanner::v1::Mutation& m) {
    return *m.mutable_insert_or_update();
  }
};

struct ReplaceOp {
  static google::spanner::v1::Mutation::Write& mutable_field(
      google::spanner::v1::Mutation& m) {
    return *m.mutable_replace();
  }
};

class DeleteMutationBuilder {
 public:
  DeleteMutationBuilder(std::string table_name, spanner::KeySet keys) {
    auto& field = *m_.proto().mutable_delete_();
    field.set_table(std::move(table_name));
    *field.mutable_key_set() = spanner_internal::ToProto(std::move(keys));
  }

  spanner::Mutation Build() const& { return m_; }
  spanner::Mutation&& Build() && { return std::move(m_); }

 private:
  spanner::Mutation m_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * A helper class to construct "insert" mutations.
 *
 * @par Example
 * @snippet samples.cc insert-mutation-builder
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
using InsertMutationBuilder =
    spanner_internal::WriteMutationBuilder<spanner_internal::InsertOp>;

/**
 * Creates a simple insert mutation for the values in @p values.
 *
 * @par Example
 * @snippet samples.cc make-insert-mutation
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
template <typename... Ts>
Mutation MakeInsertMutation(std::string table_name,
                            std::vector<std::string> columns, Ts&&... values) {
  return InsertMutationBuilder(std::move(table_name), std::move(columns))
      .EmplaceRow(std::forward<Ts>(values)...)
      .Build();
}

/**
 * A helper class to construct "update" mutations.
 *
 * @par Example
 * @snippet samples.cc update-mutation-builder
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
using UpdateMutationBuilder =
    spanner_internal::WriteMutationBuilder<spanner_internal::UpdateOp>;

/**
 * Creates a simple update mutation for the values in @p values.
 *
 * @par Example
 * @snippet samples.cc make-update-mutation
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
template <typename... Ts>
Mutation MakeUpdateMutation(std::string table_name,
                            std::vector<std::string> columns, Ts&&... values) {
  return UpdateMutationBuilder(std::move(table_name), std::move(columns))
      .EmplaceRow(std::forward<Ts>(values)...)
      .Build();
}

/**
 * A helper class to construct "insert_or_update" mutations.
 *
 * @par Example
 * @snippet samples.cc insert-or-update-mutation-builder
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
using InsertOrUpdateMutationBuilder =
    spanner_internal::WriteMutationBuilder<spanner_internal::InsertOrUpdateOp>;

/**
 * Creates a simple "insert or update" mutation for the values in @p values.
 *
 * @par Example
 * @snippet samples.cc make-insert-or-update-mutation
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
template <typename... Ts>
Mutation MakeInsertOrUpdateMutation(std::string table_name,
                                    std::vector<std::string> columns,
                                    Ts&&... values) {
  return InsertOrUpdateMutationBuilder(std::move(table_name),
                                       std::move(columns))
      .EmplaceRow(std::forward<Ts>(values)...)
      .Build();
}

/**
 * A helper class to construct "replace" mutations.
 *
 * @par Example
 * @snippet samples.cc replace-mutation-builder
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
using ReplaceMutationBuilder =
    spanner_internal::WriteMutationBuilder<spanner_internal::ReplaceOp>;

/**
 * Creates a simple "replace" mutation for the values in @p values.
 *
 * @par Example
 * @snippet samples.cc make-replace-mutation
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
template <typename... Ts>
Mutation MakeReplaceMutation(std::string table_name,
                             std::vector<std::string> columns, Ts&&... values) {
  return ReplaceMutationBuilder(std::move(table_name), std::move(columns))
      .EmplaceRow(std::forward<Ts>(values)...)
      .Build();
}

/**
 * A helper class to construct "delete" mutations.
 *
 * @par Example
 * @snippet samples.cc delete-mutation-builder
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
using DeleteMutationBuilder = spanner_internal::DeleteMutationBuilder;

/**
 * Creates a simple "delete" mutation for the values in @p keys.
 *
 * @par Example
 * @snippet samples.cc make-delete-mutation
 *
 * @see The Mutation class documentation for an overview of the Cloud Spanner
 *   mutation API
 *
 * @see https://cloud.google.com/spanner/docs/modify-mutation-api
 *   for more information about the Cloud Spanner mutation API.
 */
inline Mutation MakeDeleteMutation(std::string table_name, KeySet keys) {
  return DeleteMutationBuilder(std::move(table_name), std::move(keys)).Build();
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MUTATIONS_H
