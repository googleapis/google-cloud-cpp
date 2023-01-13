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

#include "google/cloud/storage/internal/xml_parser.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string XmlParser::CleanUpXml(std::string const& content) {
  auto ret = std::regex_replace(content, xml_decl_re_, "");
  ret = std::regex_replace(ret, xml_doctype_re_, "");
  ret = std::regex_replace(ret, xml_cdata_re_, "");
  ret = std::regex_replace(ret, xml_comment_re_, "");
  return ret;
}

StatusOr<std::shared_ptr<XmlNode>> XmlParser::ParseXml(
    std::string const& content, size_t max_xml_byte_size, size_t, size_t) {
  // Check size first
  if (content.length() > max_xml_byte_size) {
    return internal::InvalidArgumentError(
        absl::StrCat("XML exceeds the max byte size of ", max_xml_byte_size));
  }
  // Remove unnecessary bits
  auto trimmed = CleanUpXml(content);
  // And to be continued...
  return internal::UnimplementedError("not implemented");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
