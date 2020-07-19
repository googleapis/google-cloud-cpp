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

#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/version.h"
#include "absl/types/optional.h"
#include <iostream>
#include <string>
#include <vector>

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
  PatchBuilder() = default;

  /// Return the patch as a string.
  std::string ToString() const {
    if (empty()) {
      return "{}";
    }
    return patch_.dump();
  }

  bool empty() const { return patch_.empty(); }
  void clear() { patch_.clear(); }

  //@{
  /// @name Calculate the delta between the original (`lhs`) and the new (`rhs`)
  /// values and set the patch instructions accordingly.

  /// Add a string field, treat empty strings as `null`.
  PatchBuilder& AddStringField(char const* field_name, std::string const& lhs,
                               std::string const& rhs) {
    if (lhs == rhs) return *this;
    if (rhs.empty()) {
      patch_[field_name] = nullptr;
      return *this;
    }
    patch_[field_name] = rhs;
    return *this;
  }

  /**
   * Add a boolean field to the patch.
   *
   * There is no `bool` value used to represent `null`, if you want to delete
   * boolean fields use the `absl::optional<bool>` overload.
   */
  PatchBuilder& AddBoolField(char const* field_name, bool lhs, bool rhs) {
    if (lhs == rhs) return *this;
    patch_[field_name] = rhs;
    return *this;
  }

  PatchBuilder& AddIntField(char const* field_name, std::int32_t lhs,
                            std::int32_t rhs, std::int32_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }
  PatchBuilder& AddIntField(char const* field_name, std::uint32_t lhs,
                            std::uint32_t rhs, std::uint32_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }
  PatchBuilder& AddIntField(char const* field_name, std::int64_t lhs,
                            std::int64_t rhs, std::int64_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }
  PatchBuilder& AddIntField(char const* field_name, std::uint64_t lhs,
                            std::uint64_t rhs, std::uint64_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }

  /**
   * Adds a patch for a field of type @p T represented by C++ optionals.
   *
   * @tparam T the type of the field, typically `std::string`, `bool`, or some
   *     integral type.
   * @param field_name the name of the field.
   * @param lhs the previous value of the field.
   * @param rhs the new value for the field. If both @p lhs and @p rhs are empty
   *     the patch leaves the value untouched, if @p rhs is empty, create a
   *     patch that removes the previous value.
   */
  template <typename T>
  PatchBuilder& AddOptionalField(char const* field_name,
                                 absl::optional<T> const& lhs,
                                 absl::optional<T> const& rhs) {
    if (lhs == rhs) return *this;
    if (!rhs.has_value()) {
      patch_[field_name] = nullptr;
      return *this;
    }
    patch_[field_name] = *rhs;
    return *this;
  }

  /**
   * Adds a patch for a array fields.
   *
   * @tparam T the type of the field, typically `std::string`, `bool`, or
   *     some integral type.
   *
   * @param field_name the name of the field.
   * @param lhs the previous value of the field.
   * @param rhs the new value for the field. If both @p lhs and @p rhs are empty
   *     the patch leaves the value untouched, if @p rhs is empty, create a
   *     patch that removes the previous value.
   */
  template <typename T>
  PatchBuilder& AddArrayField(char const* field_name, std::vector<T> const& lhs,
                              std::vector<T> const& rhs) {
    if (lhs == rhs) return *this;
    if (rhs.empty()) {
      patch_[field_name] = nullptr;
      return *this;
    }
    patch_[field_name] = rhs;
    return *this;
  }
  //@}

  /// Add a patch for @p field_name.
  PatchBuilder& AddSubPatch(char const* field_name,
                            PatchBuilder const& builder) {
    patch_[field_name] = builder.patch_;
    return *this;
  }

  /// Create a patch that removes @p field_name
  PatchBuilder& RemoveField(char const* field_name) {
    patch_[field_name] = nullptr;
    return *this;
  }

  //@{
  /// @name Create a patch that sets fields to the given value.
  PatchBuilder& SetStringField(char const* field_name, std::string const& v) {
    patch_[field_name] = v;
    return *this;
  }

  PatchBuilder& SetBoolField(char const* field_name, bool v) {
    patch_[field_name] = v;
    return *this;
  }

  PatchBuilder& SetIntField(char const* field_name, std::int32_t v) {
    patch_[field_name] = v;
    return *this;
  }

  PatchBuilder& SetIntField(char const* field_name, std::uint32_t v) {
    patch_[field_name] = v;
    return *this;
  }

  PatchBuilder& SetIntField(char const* field_name, std::int64_t v) {
    patch_[field_name] = v;
    return *this;
  }

  PatchBuilder& SetIntField(char const* field_name, std::uint64_t v) {
    patch_[field_name] = v;
    return *this;
  }

  template <typename T>
  PatchBuilder& SetArrayField(char const* field_name, std::vector<T> const& v) {
    patch_[field_name] = v;
    return *this;
  }
  //@}

 private:
  /// Refactor the patch fields.
  template <typename Integer>
  PatchBuilder& AddIntegerField(char const* field_name, Integer lhs,
                                Integer rhs, Integer null_value) {
    if (lhs == rhs) return *this;
    if (rhs == null_value) {
      patch_[field_name] = nullptr;
      return *this;
    }
    patch_[field_name] = rhs;
    return *this;
  }

  nl::json patch_;
};
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PATCH_BUILDER_H
