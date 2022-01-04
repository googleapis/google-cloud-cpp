// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_UTILS_ASYNC_OPERATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_UTILS_ASYNC_OPERATION_H

#include "google/cloud/async_operation.h"
#include "google/cloud/version.h"
#warning \
    "We moved the helpers in the google::cloud::grpc_utils namespace to " \
      "`google::cloud`, please use them directly. The aliases to maintain " \
      "backwards compatibility will be removed on or about 2023-01-01. "    \
      "More details at "                                                    \
      "https://github.com/googleapis/google-cloud-cpp/issues/3932")

namespace google {
namespace cloud {
namespace grpc_utils {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/// Use a type alias to maintain API backwards compatibility.
using AsyncOperation = ::google::cloud::AsyncOperation;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace grpc_utils
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_UTILS_ASYNC_OPERATION_H
