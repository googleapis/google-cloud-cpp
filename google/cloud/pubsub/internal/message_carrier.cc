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
#include "google/cloud/options.h"
#include "absl/strings/match.h"
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/span_startoptions.h>
namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

opentelemetry::nostd::string_view MessageCarrier::Get(
    opentelemetry::nostd::string_view key) const noexcept {
  auto result =
      internal::NoExceptAction<opentelemetry::nostd::string_view>([&] {
        auto message_attributes = message_.attributes();

        auto it = message_attributes.find(
            absl::StrCat("googclient_", std::string(key.data(), key.size())));
        if (it != message_attributes.end()) {
          return opentelemetry::nostd::string_view(it->second);
        }
        return opentelemetry::nostd::string_view{};
      });
  if (result) return *std::move(result);

  return opentelemetry::nostd::string_view{};
}

void MessageCarrier::Set(opentelemetry::nostd::string_view key,
                         opentelemetry::nostd::string_view value) noexcept {
  internal::NoExceptAction([this, key, value] {
    SetAttribute(
        absl::StrCat("googclient_", std::string(key.data(), key.size())),
        value.data(), message_);
  });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
