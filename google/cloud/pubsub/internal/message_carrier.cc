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

#include "google/cloud/pubsub/internal/message_carrier.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/internal/opentelemetry.h"
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

opentelemetry::nostd::string_view MessageCarrier::Get(
    opentelemetry::nostd::string_view key) const noexcept {
  auto result =
      internal::NoExceptAction<opentelemetry::nostd::string_view>([this, key] {
        auto value = GetAttribute(
            absl::StrCat("googclient_",
                         absl::string_view(key.data(), key.size())),
            message_);
        return opentelemetry::nostd::string_view(value.data(), value.size());
      });
  return result ? *result : opentelemetry::nostd::string_view{};
}

void MessageCarrier::Set(opentelemetry::nostd::string_view key,
                         opentelemetry::nostd::string_view value) noexcept {
  internal::NoExceptAction([this, key, value] {
    SetAttribute(
        absl::StrCat("googclient_", absl::string_view(key.data(), key.size())),
        std::string(value.data(), value.size()), message_);
  });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
