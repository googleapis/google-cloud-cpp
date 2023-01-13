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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_PARSER_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_PARSER_OPTIONS_H

#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Use with `google::cloud::Options` to configure the XML parser.
 *
 * The maximum size of the source string in bytes.
 */
struct XmlParserSourceMaxBytes {
  using Type = std::size_t;
};

/**
 * Use with `google::cloud::Options` to configure the XML parser.
 *
 * The maximum number of total Nodes in the XML tree.
 */
struct XmlParserMaxNodeNum {
  using Type = std::size_t;
};

/**
 * Use with `google::cloud::Options` to configure the XML parser.
 *
 * The maximum depth of the node in the XML tree.
 */
struct XmlParserMaxNodeDepth {
  using Type = std::size_t;
};

using XmlParserOptionsList =
    OptionList<XmlParserSourceMaxBytes, XmlParserMaxNodeNum,
               XmlParserMaxNodeDepth>;

Options XmlParserDefaultOptions(Options options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_PARSER_OPTIONS_H
