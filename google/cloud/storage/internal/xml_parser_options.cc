
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

#include "google/cloud/storage/internal/xml_parser_options.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

constexpr std::size_t kDefaultMaxSourceSize = 1024 * 1024 * 1024;  // 1GiB
constexpr std::size_t kDefaultMaxXmlNodeCount = 20000;
constexpr std::size_t kDefaultMaxXmlNodeDepth = 50;

}  // namespace

Options XmlParserDefaultOptions(Options options) {
  if (!options.has<XmlParserMaxSourceSize>()) {
    options.set<XmlParserMaxSourceSize>(kDefaultMaxSourceSize);
  }
  if (!options.has<XmlParserMaxNodeCount>()) {
    options.set<XmlParserMaxNodeCount>(kDefaultMaxXmlNodeCount);
  }
  if (!options.has<XmlParserMaxNodeDepth>()) {
    options.set<XmlParserMaxNodeDepth>(kDefaultMaxXmlNodeDepth);
  }
  return options;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
