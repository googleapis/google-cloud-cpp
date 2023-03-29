// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_H

#include "google/cloud/bigquery/v2/minimal/internal/job_configuration.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

struct JobStatus {
  ErrorProto error_result;
  std::vector<ErrorProto> errors;
  std::string state;
};

struct JobReference {
  std::string project_id;
  std::string job_id;
  std::string location;
};

struct Job {
  std::string kind;
  std::string etag;
  std::string id;
  std::string self_link;
  std::string user_email;

  JobStatus status;
  JobReference reference;
  JobConfiguration configuration;

  std::string DebugString(TracingOptions const& options) const;
};

struct ListFormatJob {
  std::string id;
  std::string kind;
  std::string user_email;
  std::string state;
  std::string principal_subject;

  JobReference reference;
  JobConfiguration configuration;
  JobStatus status;

  ErrorProto error_result;

  std::string DebugString(TracingOptions const& options) const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(JobStatus, error_result, errors,
                                                state);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(JobReference, project_id,
                                                job_id, location);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Job, kind, etag, id, self_link,
                                                user_email, status, reference,
                                                configuration);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ListFormatJob, id, kind,
                                                user_email, state,
                                                principal_subject, reference,
                                                configuration, status,
                                                error_result);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_H
