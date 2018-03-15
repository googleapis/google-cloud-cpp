// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_READ_MODIFY_WRITE_RULE_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_READ_MODIFY_WRITE_RULE_H_

#include "bigtable/client/version.h"
#include <google/bigtable/v2/data.pb.h>
#include <chrono>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the interfaces to create ReadWriteModifyRule operations.
 *
 * Cloud Bigtable has operations to perform atomic updates to a row, such as
 * incrementing an integer value or appending to a string value. The changes are
 * represented by a ReadModifyWriteRule operations. One or much such operations
 * can be sent in a single request. This class helps users create the operations
 * through a more idiomatic C++ interface.
 */
class ReadModifyWriteRule {
 public:
  ReadModifyWriteRule(ReadModifyWriteRule&& rhs) noexcept = default;
  ReadModifyWriteRule& operator=(ReadModifyWriteRule&& rhs) noexcept = default;
  ReadModifyWriteRule(ReadModifyWriteRule const& rhs) = default;
  ReadModifyWriteRule& operator=(ReadModifyWriteRule const& rhs) = default;

  /// Create an operation that appends a string value.
  static ReadModifyWriteRule AppendValue(std::string family_name,
                                         std::string column_qualifier,
                                         std::string value) {
    ReadModifyWriteRule tmp;
    tmp.rule_.set_family_name(std::move(family_name));
    tmp.rule_.set_column_qualifier(std::move(column_qualifier));
    tmp.rule_.set_append_value(std::move(value));
    return tmp;
  }

  /// Create an operation that increments an integer value.
  static ReadModifyWriteRule IncrementAmount(std::string family_name,
                                             std::string column_qualifier,
                                             std::int64_t amount) {
    ReadModifyWriteRule tmp;
    tmp.rule_.set_family_name(std::move(family_name));
    tmp.rule_.set_column_qualifier(std::move(column_qualifier));
    tmp.rule_.set_increment_amount(amount);
    return tmp;
  }

  /// Return the filter expression as a protobuf.
  google::bigtable::v2::ReadModifyWriteRule as_proto() const { return rule_; }

  /// Move out the underlying protobuf value.
  google::bigtable::v2::ReadModifyWriteRule as_proto_move() {
    return std::move(rule_);
  }

 private:
  /**
   * Create an empty operation.
   *
   * Empty operations are not usable, so this constructor is private.
   */
  ReadModifyWriteRule() {}

 private:
  google::bigtable::v2::ReadModifyWriteRule rule_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_READ_MODIFY_WRITE_RULE_H_
