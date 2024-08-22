// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REQUEST_PROJECT_ID_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REQUEST_PROJECT_ID_H

#include "google/cloud/storage/options.h"
#include "google/cloud/storage/override_default_project.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::string> GetOverrideDefaultProject(internal::ErrorInfoBuilder ei);

template <typename... RequestOptions>
StatusOr<std::string> GetOverrideDefaultProject(internal::ErrorInfoBuilder ei,
                                                RequestOptions&&... ro);

template <typename... RequestOptions>
StatusOr<std::string> GetOverrideDefaultProjectImpl(
    internal::ErrorInfoBuilder ei, storage::OverrideDefaultProject const& o,
    RequestOptions&&... ro) {
  if (o.has_value()) return o.value();
  return GetOverrideDefaultProject(std::move(ei),
                                   std::forward<RequestOptions>(ro)...);
}

template <typename O, typename... RequestOptions>
StatusOr<std::string> GetOverrideDefaultProjectImpl(
    internal::ErrorInfoBuilder ei, O&&, RequestOptions&&... ro) {
  return GetOverrideDefaultProject(std::move(ei),
                                   std::forward<RequestOptions>(ro)...);
}

template <typename... RequestOptions>
StatusOr<std::string> GetOverrideDefaultProject(internal::ErrorInfoBuilder ei,
                                                RequestOptions&&... ro) {
  return GetOverrideDefaultProjectImpl(std::move(ei),
                                       std::forward<RequestOptions>(ro)...);
}

/**
 * Returns the effective project id for a request.
 *
 * Some RPCs in GCS need a project id, and use the default configured via the
 * `google::cloud::Options` configured in the client. Before we introduced
 * per-call `Options` parameters GCS had "request options" as a variadic
 * list of template arguments. One of the request options could be of type
 * `OverrideDefaultProject` and override the default. And then we introduced
 * the per-call `Options`.
 *
 * This function refactors the code to extract the default project id. It
 * returns an error if the project id is not configured, assuming it is a
 * required value for the caller.
 */
template <typename... RequestOptions>
StatusOr<std::string> RequestProjectId(
    internal::ErrorInfoBuilder ei, Options const& options,
    RequestOptions const&... request_options) {
  auto project_id =
      GetOverrideDefaultProject(std::move(ei), request_options...);
  if (project_id) return project_id;
  if (!options.has<storage::ProjectIdOption>()) return project_id;
  return options.get<storage::ProjectIdOption>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REQUEST_PROJECT_ID_H
