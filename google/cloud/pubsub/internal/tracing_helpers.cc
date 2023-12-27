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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/pubsub/internal/tracing_helpers.h"
#include "google/cloud/pubsub/version.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/span_startoptions.h"
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

opentelemetry::trace::StartSpanOptions RootStartSpanOptions() {
  opentelemetry::trace::StartSpanOptions options;
  opentelemetry::context::Context root_context;
  // TODO(#13287): Use the constant instead of the string.
  // Setting a span as a root span was added in OTel v1.13+. It is a no-op for
  // earlier versions.
  options.parent = root_context.SetValue(
      /*opentelemetry::trace::kIsRootSpanKey=*/"is_root_span", true);
  return options;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
