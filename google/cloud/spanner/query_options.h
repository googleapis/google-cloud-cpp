// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_QUERY_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_QUERY_OPTIONS_H

#include "google/cloud/spanner/request_priority.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/optional.h"
#include "absl/types/optional.h"
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * These QueryOptions allow users to configure features about how their SQL
 * queries executes on the server.
 *
 * @see https://cloud.google.com/spanner/docs/reference/rest/v1/QueryOptions
 * @see http://cloud/spanner/docs/query-optimizer/manage-query-optimizer
 */
class QueryOptions {
 public:
  QueryOptions() = default;
  QueryOptions(QueryOptions const&) = default;
  QueryOptions& operator=(QueryOptions const&) = default;
  QueryOptions(QueryOptions&&) = default;
  QueryOptions& operator=(QueryOptions&&) = default;

  /// Returns the optimizer version
  absl::optional<std::string> const& optimizer_version() const {
    return optimizer_version_;
  }

  /**
   * Sets the optimizer version to the specified integer string. Setting to
   * the empty string will use the database default. Use the string "latest" to
   * use the latest available optimizer version.
   */
  QueryOptions& set_optimizer_version(absl::optional<std::string> version) {
    optimizer_version_ = std::move(version);
    return *this;
  }

  /// Returns the optimizer statistics package
  absl::optional<std::string> const& optimizer_statistics_package() const {
    return optimizer_statistics_package_;
  }

  /**
   * Sets the optimizer statistics package to the specified string. Setting to
   * the empty string will use the database default.
   */
  QueryOptions& set_optimizer_statistics_package(
      absl::optional<std::string> stats_package) {
    optimizer_statistics_package_ = std::move(stats_package);
    return *this;
  }

  /// Returns the request priority.
  absl::optional<RequestPriority> const& request_priority() const {
    return request_priority_;
  }

  /// Sets the request priority.
  QueryOptions& set_request_priority(absl::optional<RequestPriority> priority) {
    request_priority_ = std::move(priority);
    return *this;
  }

  /// Returns the request tag.
  absl::optional<std::string> const& request_tag() const {
    return request_tag_;
  }

  /// Sets the request tag.
  QueryOptions& set_request_tag(absl::optional<std::string> tag) {
    request_tag_ = std::move(tag);
    return *this;
  }

  friend bool operator==(QueryOptions const& a, QueryOptions const& b) {
    return a.request_priority_ == b.request_priority_ &&
           a.request_tag_ == b.request_tag_ &&
           a.optimizer_version_ == b.optimizer_version_ &&
           a.optimizer_statistics_package_ == b.optimizer_statistics_package_;
  }

  friend bool operator!=(QueryOptions const& a, QueryOptions const& b) {
    return !(a == b);
  }

 private:
  // Note: If you add an attribute here, remember to update the implementation
  // of Client::OverlayQueryOptions().
  absl::optional<std::string> optimizer_version_;
  absl::optional<std::string> optimizer_statistics_package_;
  absl::optional<RequestPriority> request_priority_;
  absl::optional<std::string> request_tag_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_QUERY_OPTIONS_H
