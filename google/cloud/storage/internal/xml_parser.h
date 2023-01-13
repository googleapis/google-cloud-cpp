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
#include "google/cloud/storage/internal/xml_parser_options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/make_status.h"
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
 */
class XmlParser {
  constexpr static auto const kXmlDeclRe = R"(^<\?xml[^>]*\?>)";
  constexpr static auto const kXmlDoctypeRe =
      R"(<!DOCTYPE[^>[]*(\[[^\]]*\])?>)";
  constexpr static auto const kXmlCdataRe = R"(<!\[CDATA\[[^>]*\]\]>)";
  constexpr static auto const kXmlCommentRe = R"(<!--[^>]*-->)";

 public:
  /// Factory method
  static std::shared_ptr<XmlParser> Create() {
    return std::shared_ptr<XmlParser>{new XmlParser};
  }

  /// Clean up the given XML string.
  std::string CleanUpXml(std::string const& content);
  /// Parse the given string and return an XML tree.
  StatusOr<std::shared_ptr<XmlNode>> ParseXml(std::string const& content,
                                              Options = {});

 private:
  XmlParser()
      : xml_decl_re_{kXmlDeclRe},
        xml_doctype_re_{kXmlDoctypeRe},
        xml_cdata_re_{kXmlCdataRe},
        xml_comment_re_{kXmlCommentRe} {}

  std::regex xml_decl_re_;
  std::regex xml_doctype_re_;
  std::regex xml_cdata_re_;
  std::regex xml_comment_re_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_PARSER_H
