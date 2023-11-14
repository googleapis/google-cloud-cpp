// Copyright 2019 Google LLC
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

#include "google/cloud/internal/log_wrapper.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

void LogRequest(absl::string_view where, absl::string_view args,
                absl::string_view message) {
  GCP_LOG(DEBUG) << where << '(' << args << ')' << " << " << message;
}

Status LogResponse(Status response, absl::string_view where,
                   absl::string_view args, TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << '(' << args << ')'
                 << " >> status=" << DebugString(response, options);
  return response;
}

void LogResponseFuture(std::future_status status, absl::string_view where,
                       absl::string_view args,
                       TracingOptions const& /*options*/) {
  GCP_LOG(DEBUG) << where << '(' << args << ')'
                 << " >> future_status=" << DebugFutureStatus(status);
}

future<Status> LogResponse(future<Status> response, std::string where,
                           std::string args, TracingOptions const& options) {
  LogResponseFuture(response.wait_for(std::chrono::microseconds(0)), where,
                    args, options);
  return response.then([where = std::move(where), args = std::move(args),
                        options = std::move(options)](auto f) {
    return LogResponse(f.get(), where, args, options);
  });
}

void LogResponsePtr(bool not_null, absl::string_view where,
                    absl::string_view args, TracingOptions const& /*options*/) {
  GCP_LOG(DEBUG) << where << '(' << args << ')' << " >> "
                 << (not_null ? "not " : "") << "null";
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
