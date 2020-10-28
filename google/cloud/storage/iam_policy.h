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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IAM_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IAM_POLICY_H

#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents a google::type::Expr.
 *
 * This is a textual representation of an expression in Common Expression
 * Language (CEL) syntax.
 */
class NativeExpression {
 public:
  /**
   * Create a `NativeExpression`.
   *
   * @param expression the expression in Common Expression Language.
   * @param title an optional title for the expression, i.e. a short string
   *     describing its purpose.
   * @param description an optional description of the expression. This is a
   *     longer text which describes the expression, e.g. when hovered over it
   *     in a UI.
   * @param location an optional string indicating the location of the
   *     expression for error reporting, e.g. a file name and a position in the
   *     file.
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  NativeExpression(std::string expression, std::string title = "",
                   std::string description = "", std::string location = "");
  ~NativeExpression();

  NativeExpression(NativeExpression const& other);
  NativeExpression& operator=(NativeExpression const& other);

  // This have to be declared explicitly and defined out of line because `Impl`
  // is incomplete at this point.
  NativeExpression(NativeExpression&&) noexcept;
  NativeExpression& operator=(NativeExpression&&) noexcept;

  std::string expression() const;
  void set_expression(std::string expression);
  std::string title() const;
  void set_title(std::string title);
  std::string description() const;
  void set_description(std::string description);
  std::string location() const;
  void set_location(std::string location);

 private:
  struct Impl;
  explicit NativeExpression(std::unique_ptr<Impl> impl);
  friend class NativeIamPolicy;
  friend class NativeIamBinding;
  std::unique_ptr<Impl> pimpl_;
};

std::ostream& operator<<(std::ostream& stream, NativeExpression const&);

/**
 * Represents a Binding which associates a `member` with a particular `role`
 * which can be used for Identity and Access management for Cloud Platform
 * Resources.
 *
 * For more information about a Binding please refer to:
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Binding
 *
 * Compared to `IamBinding`, `NativeIamBinding` is a more future-proof
 * solution - it gracefully tolerates changes in the underlying protocol.
 * If IamBinding contains more fields than just a role and members, in the
 * future, `NativeIamBinding` will preserve them (contrary to IamBinding).
 */
class NativeIamBinding {
 public:
  NativeIamBinding(std::string role, std::vector<std::string> members);
  NativeIamBinding(std::string role, std::vector<std::string> members,
                   NativeExpression condition);
  ~NativeIamBinding();

  NativeIamBinding(NativeIamBinding const& other);
  NativeIamBinding& operator=(NativeIamBinding const& other);

  // This have to be declared explicitly and defined out of line because `Impl`
  // is incomplete at this point.
  NativeIamBinding(NativeIamBinding&&) noexcept;
  NativeIamBinding& operator=(NativeIamBinding&&) noexcept;

  std::string role() const;
  void set_role(std::string role);
  std::vector<std::string> const& members() const;
  std::vector<std::string>& members();
  NativeExpression const& condition() const;
  NativeExpression& condition();
  void set_condition(NativeExpression condition);
  bool has_condition() const;
  void clear_condition();

 private:
  struct Impl;
  explicit NativeIamBinding(std::unique_ptr<Impl> impl);
  friend class NativeIamPolicy;
  std::unique_ptr<Impl> pimpl_;
};

std::ostream& operator<<(std::ostream& os, NativeIamBinding const& binding);

/**
 * Represent the result of a GetIamPolicy or SetIamPolicy request.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a AIM policies.
 *
 * @see https://tools.ietf.org/html/rfc7232#section-2.3 for more information
 *     about ETags.
 *
 * Compared to `IamPolicy`, `NativeIamPolicy` is a more future-proof
 * solution - it gracefully tolerates changes in the underlying protocol.
 * If IamPolicy is extended with additional fields in the future,
 * `NativeIamPolicy` will preserve them (contrary to IamPolicy).
 */
class NativeIamPolicy {
 public:
  explicit NativeIamPolicy(std::vector<NativeIamBinding> bindings,
                           std::string etag = "", std::int32_t version = 0);
  NativeIamPolicy(NativeIamPolicy const& other);
  ~NativeIamPolicy();

  static StatusOr<NativeIamPolicy> CreateFromJson(std::string const& json_rep);
  std::string ToJson() const;

  NativeIamPolicy& operator=(NativeIamPolicy const& other);

  std::int32_t version() const;
  void set_version(std::int32_t version);
  std::string etag() const;
  void set_etag(std::string etag);
  std::vector<NativeIamBinding>& bindings();
  std::vector<NativeIamBinding> const& bindings() const;

 private:
  struct Impl;
  explicit NativeIamPolicy(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> pimpl_;
};

std::ostream& operator<<(std::ostream& os, NativeIamPolicy const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IAM_POLICY_H
