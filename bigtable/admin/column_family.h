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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_COLUMN_FAMILY_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_COLUMN_FAMILY_H_

#include "bigtable/client/version.h"

#include <chrono>
#include <memory>

#include <google/bigtable/admin/v2/table.pb.h>

#include "bigtable/client/detail/conjunction.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Implement a thin wrapper around google::bigtable::admin::v2::GcRule.
 *
 * Provides functions to create GcRules in a convenient form.
 */
class GcRule {
 public:
  /// Create a garbage collection rule that keeps the last @p n versions.
  static GcRule MaxNumVersions(std::int32_t n) {
    GcRule tmp;
    tmp.gc_rule_.set_max_num_versions(n);
    return tmp;
  }

  /**
   * Return a garbage collection rule that deletes cells in a column older than
   * the given duration.
   *
   * The function accepts any instantiation of std::chrono::duration<> for the
   * @p duration parameter.
   *
   * @tparam Rep the Rep tparam for @p duration type.
   * @tparam Period the Period tparam for @p duration type.
   */
  template <typename Rep, typename Period>
  static GcRule MaxAge(std::chrono::duration<Rep, Period> duration) {
    GcRule tmp;
    auto& max_age = *tmp.gc_rule_.mutable_max_age();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    max_age.set_seconds(seconds.count());
    std::chrono::nanoseconds nanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration) -
        seconds;
    max_age.set_nanos(static_cast<std::int32_t>(nanos.count()));
    return tmp;
  }

  /**
   * Return a GcRule that deletes cells if all the rules passed in would delete
   * the cells.
   *
   * @tparam GcRuleTypes the type of the GC rule arguments.  They must all be
   *     convertible to GcRule.
   * @param gc_rules the set of GC rules.
   */
  template <typename... GcRuleTypes>
  static GcRule Intersection(GcRuleTypes&&... gc_rules) {
    // This ugly thing provides a better compile-time error message than just
    // letting the compiler figure things out N levels deep as it recurses on
    // add_intersection().
    static_assert(
        detail::conjunction<std::is_convertible<GcRuleTypes, GcRule>...>::value,
        "The arguments to Intersection must be convertible to GcRule");
    GcRule tmp;
    auto& intersection = *tmp.gc_rule_.mutable_intersection();
    append_rules(intersection, std::forward<GcRuleTypes>(gc_rules)...);
    return tmp;
  }

  /**
   * Return a GcRule that deletes cells if any the rules passed in would delete
   * the cells.
   *
   * @tparam GcRuleTypes the type of the GC rule arguments.  They must all be
   *     convertible to GcRule.
   * @param gc_rules the set of GC rules.
   */
  template <typename... GcRuleTypes>
  static GcRule Union(GcRuleTypes&&... gc_rules) {
    // This ugly thing provides a better compile-time error message than just
    // letting the compiler figure things out N levels deep as it recurses on
    // add_intersection().
    static_assert(
        detail::conjunction<std::is_convertible<GcRuleTypes, GcRule>...>::value,
        "The arguments to Union must be convertible to GcRule");
    GcRule tmp;
    auto& union_ = *tmp.gc_rule_.mutable_union_();
    append_rules(union_, std::forward<GcRuleTypes>(gc_rules)...);
    return tmp;
  }

  /// Convert to the proto form.
  google::bigtable::admin::v2::GcRule as_proto() const { return gc_rule_; }

  /// Move the internal proto out.
  google::bigtable::admin::v2::GcRule as_proto_move() {
    return std::move(gc_rule_);
  }

  //@{
  /// @name Use default constructors and assignments.
  GcRule(GcRule&& rhs) noexcept = default;
  GcRule& operator=(GcRule&& rhs) noexcept = default;
  GcRule(GcRule const& rhs) = default;
  GcRule& operator=(GcRule const& rhs) = default;
  //@}

 private:
  GcRule() {}

  /// Append @p head and recursively append @tail to @p intersection.
  template <typename GcOperation, typename... GcRuleTypes>
  static void append_rules(
      GcOperation& operation, GcRule&& head, GcRuleTypes&&... tail) {
    operation.add_rules()->Swap(&head.gc_rule_);
    append_rules(operation, std::forward<GcRuleTypes>(tail)...);
  }

  /// Terminate the recursion for append_rules().
  template <typename GcOperation>
  static void append_rules(GcOperation& operation) {}

 private:
  google::bigtable::admin::v2::GcRule gc_rule_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_COLUMN_FAMILY_H_
