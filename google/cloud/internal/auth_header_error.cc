// Copyright 2022 Google LLC
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

#include "google/cloud/internal/auth_header_error.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Status AuthHeaderError(Status status) {
  if (status.ok()) return status;
  auto constexpr kText =
      "Could not create a OAuth2 access token to authenticate the request."
      " The request was not sent, as such an access token is required to"
      " complete the request successfully. Learn more about Google Cloud"
      " authentication at https://cloud.google.com/docs/authentication."
      " The underlying error message was: ";
  auto message = kText + status.message();
  return Status{status.code(), message, status.error_info()};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
