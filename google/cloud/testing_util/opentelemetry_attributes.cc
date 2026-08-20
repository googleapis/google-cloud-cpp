// Copyright 2026 Google LLC
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

#include "google/cloud/testing_util/opentelemetry_attributes.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/nostd/variant.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

std::unordered_map<std::string, std::string> MakeAttributesMap(
    opentelemetry::common::KeyValueIterable const& attributes) {
  std::unordered_map<std::string, std::string> m;
  attributes.ForEachKeyValue([&](opentelemetry::nostd::string_view k,
                                 opentelemetry::common::AttributeValue v) {
    if (opentelemetry::nostd::holds_alternative<
            opentelemetry::nostd::string_view>(v)) {
      m.emplace(
          std::string{k},
          opentelemetry::nostd::get<opentelemetry::nostd::string_view>(v));
    }
    return true;
  });
  return m;
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
