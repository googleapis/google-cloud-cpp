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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_WRAPPER_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_WRAPPER_HELPERS_H

#include "google/cloud/status.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <google/protobuf/message.h>
#include <future>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string DebugString(google::protobuf::Message const& m,
                        TracingOptions const& options);

std::string DebugString(Status const& status, TracingOptions const& options);

std::string DebugString(std::string s, TracingOptions const& options);

char const* DebugFutureStatus(std::future_status s);

// Create a unique ID that can be used to match asynchronous requests/response
// pairs.
std::string RequestIdForLogging();

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_WRAPPER_HELPERS_H
