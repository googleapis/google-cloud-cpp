// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PATCH_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PATCH_BUILDER_H

#include "google/cloud/storage/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Prepares a patch for the '<Resource Type>: patch' APIs in Google Cloud
 * Storage.
 *
 * There are multiple APIs in Google Cloud Storage that receive patches. The
 * format for these patches is described in:
 *
 * https://cloud.google.com/storage/docs/json_api/v1/how-tos/performance#patch
 *
 * At a high level: fields present in the patch are set to their new values,
 * unless the field has value `null`, in which case the field is removed.
 */
class PatchBuilder {
 public:
  PatchBuilder();
  ~PatchBuilder();

  PatchBuilder(PatchBuilder const& other);
  PatchBuilder& operator=(PatchBuilder const& other);

  // This have to be declared explicitly and defined out of line because `Impl`
  // is incomplete at this point.
  PatchBuilder(PatchBuilder&&) noexcept;
  PatchBuilder& operator=(PatchBuilder&&) noexcept;

  /// Return the patch as a string.
  std::string ToString() const;

  bool empty() const;
  void clear();

  //@{
  /// @name Calculate the delta between the original (`lhs`) and the new (`rhs`)
  /// values and set the patch instructions accordingly.

  /// Add a string field, treat empty strings as `null`.
  PatchBuilder& AddStringField(char const* field_name, std::string const& lhs,
                               std::string const& rhs);

  /**
   * Add a boolean field to the patch.
   *
   * There is no `bool` value used to represent `null`, if you want to delete
   * boolean fields use the `absl::optional<bool>` overload.
   */
  PatchBuilder& AddBoolField(char const* field_name, bool lhs, bool rhs);

  PatchBuilder& AddIntField(char const* field_name, std::int32_t lhs,
                            std::int32_t rhs, std::int32_t null_value = 0);
  PatchBuilder& AddIntField(char const* field_name, std::uint32_t lhs,
                            std::uint32_t rhs, std::uint32_t null_value = 0);
  PatchBuilder& AddIntField(char const* field_name, std::int64_t lhs,
                            std::int64_t rhs, std::int64_t null_value = 0);
  PatchBuilder& AddIntField(char const* field_name, std::uint64_t lhs,
                            std::uint64_t rhs, std::uint64_t null_value = 0);
  //@}

  /// Add a patch for @p field_name.
  PatchBuilder& AddSubPatch(char const* field_name,
                            PatchBuilder const& builder);

  /// Create a patch that removes @p field_name
  PatchBuilder& RemoveField(char const* field_name);

  //@{
  /// @name Create a patch that sets fields to the given value.
  PatchBuilder& SetStringField(char const* field_name, std::string const& v);

  PatchBuilder& SetBoolField(char const* field_name, bool v);

  PatchBuilder& SetIntField(char const* field_name, std::int32_t v);

  PatchBuilder& SetIntField(char const* field_name, std::uint32_t v);

  PatchBuilder& SetIntField(char const* field_name, std::int64_t v);

  PatchBuilder& SetIntField(char const* field_name, std::uint64_t v);
  //@}

  /**
   * Adds an array field to the patch.
   *
   * We accept a @p to avoid having a `nlohmann::json` parameter in the header,
   * with the tradeoff of this being slow.
   *
   * @par Example
   *
   * @code
   * PatchBuilder builder;
   * nlohmann::json array = nlohmann::json::array();
   * array.emplace_back("value");
   * builder.SetFieldArray("field", array.dump());
   * @endcode
   */
  PatchBuilder& SetArrayField(char const* field_name,
                              std::string const& json_stringified_object);

 private:
  struct Impl;
  explicit PatchBuilder(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> pimpl_;
};
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PATCH_BUILDER_H
