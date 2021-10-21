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

#include "google/cloud/bigtable/table_config.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// NOLINTNEXTLINE(readability-identifier-naming)
::google::bigtable::admin::v2::CreateTableRequest TableConfig::as_proto() && {
  // As a challenge, we implement the strong exception guarantee in this
  // function.
  // First create a temporary value to hold intermediate computations.
  ::google::bigtable::admin::v2::CreateTableRequest tmp;
  auto& table = *tmp.mutable_table();
  // Make sure there are nodes to receive all the values in column_families_.
  auto& families = *table.mutable_column_families();
  for (auto const& kv : column_families_) {
    families[kv.first].mutable_gc_rule()->Clear();
  }
  // Make sure there is space to receive all the values in initial_splits_.
  tmp.mutable_initial_splits()->Reserve(
      static_cast<int>(initial_splits_.size()));
  // Copy the granularity.
  table.set_granularity(timestamp_granularity());

  // None of the operations that follow can fail, they are all `noexcept`:
  for (auto& kv : column_families_) {
    *families[kv.first].mutable_gc_rule() = std::move(kv.second).as_proto();
  }
  for (auto& split : initial_splits_) {
    tmp.add_initial_splits()->set_key(std::move(split));
  }
  initial_splits_.clear();
  column_families_.clear();
  granularity_ = TIMESTAMP_GRANULARITY_UNSPECIFIED;
  return tmp;
}

// Disable clang-tidy warnings for these two definitions. clang-tidy is confused
// it claims, for example:
//   error: redundant 'MILLIS' declaration
//   [readability-redundant-declaration,-warnings-as-errors]
// But these are not declarations, these are definitions of variables that are
// odr-used in at least the tests:
//    http://en.cppreference.com/w/cpp/language/definition
// In C++17 these would be inline variables and we would not care, but alas! we
// are sticking to C++11 (for good reasons).
constexpr TableConfig::TimestampGranularity TableConfig::MILLIS;  // NOLINT
constexpr TableConfig::TimestampGranularity
    TableConfig::TIMESTAMP_GRANULARITY_UNSPECIFIED;  // NOLINT

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
