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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/internal/rest_carrier.h"
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

opentelemetry::nostd::string_view RestCarrier::Get(
    opentelemetry::nostd::string_view) const noexcept {
  GCP_LOG(FATAL) << __func__ << " should never be called";
  // Since the client is never extracting data from the REST headers, we are not
  // implementing Get and returning a null string view.
  return opentelemetry::nostd::string_view();
}

void RestCarrier::Set(opentelemetry::nostd::string_view key,
                      opentelemetry::nostd::string_view value) noexcept {
  internal::NoExceptAction([this, key, value] {
    context_.AddHeader(std::string(key.data(), key.size()),
                       std::string(value.data(), value.size()));
  });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
