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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_RETRY_TRAITS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_RETRY_TRAITS_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace iam_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
/// Define the gRPC status code semantics for retrying requests.
struct IAMCredentialsRetryTraits {
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return status.code() != StatusCode::kOk &&
           status.code() != StatusCode::kDeadlineExceeded &&
           status.code() != StatusCode::kUnavailable;
  }
};

struct IAMRetryTraits {
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return status.code() != StatusCode::kOk &&
           status.code() != StatusCode::kDeadlineExceeded &&
           status.code() != StatusCode::kUnavailable;
  }
};

}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace iam_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_RETRY_TRAITS_H
