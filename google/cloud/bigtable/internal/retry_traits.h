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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RETRY_TRAITS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RETRY_TRAITS_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/status.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// An adapter to use `grpc::Status` with the `google::cloud::*Policies`.
struct SafeGrpcRetry {
  static inline bool IsOk(Status const& status) { return status.ok(); }
  static inline bool IsTransientFailure(Status const& status) {
    auto const code = status.code();
    return code == StatusCode::kAborted || code == StatusCode::kUnavailable ||
           google::cloud::internal::IsTransientInternalError(status);
  }
  static bool IsPermanentFailure(Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }

  // TODO(#2344) - remove ::grpc::Status version.
  static inline bool IsOk(grpc::Status const& status) { return status.ok(); }
  static inline bool IsTransientFailure(grpc::Status const& status) {
    return IsTransientFailure(MakeStatusFromRpcError(status));
  }
  static bool IsPermanentFailure(grpc::Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RETRY_TRAITS_H
