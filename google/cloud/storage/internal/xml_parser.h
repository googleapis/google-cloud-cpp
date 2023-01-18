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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_PARSER_H

#include "google/cloud/storage/internal/xml_node.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <regex>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An Xml parser implementation for [GCS MPU]
 *
 * [GCS MPU]: https://cloud.google.com/storage/docs/multipart-uploads
 *
 * Note: This is not a general purpose XML parser. It is only intended to
 * parse XML responses from the [GCS MPU]. It does not support many XML
 * features.
 *
 * As a defence to DOS type attacks, the parser has several limits. The default
 * values of these limits are large enough for API responses from [GCS MPU], but
 * in case you need to configure these limits, use the following options:
 * `XmlParserMaxSourceSize`, `XmlParserMaxNodeCount`, and
 * `XmlParserMaxNodeDepth`. See
 * `google::cloud::storage::internal::xml_parser_options.h` for the default
 * values of these limits.
 */
class XmlParser {
 public:
  /// Factory method
  static std::shared_ptr<XmlParser> Create() {
    return std::shared_ptr<XmlParser>{new XmlParser};
  }

  /// Parse the given string and return an XML tree.
  StatusOr<std::shared_ptr<XmlNode>> Parse(std::string const& content,
                                           Options = {});

 private:
  XmlParser() = default;

  std::regex unneeded_re_{
      absl::StrCat("(",
                   R"(^<\?xml[^>]*\?>)",  // XML declaration
                   "|",
                   R"(<!DOCTYPE[^>[]*(\[[^\]]*\])?>)",  // DTD(DOCTYPE)
                   "|",
                   R"(<!\[CDATA\[[^>]*\]\]>)",  // CDATA
                   "|",
                   R"(<!--[^>]*-->)",  // XML comments
                   ")")};
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_PARSER_H
