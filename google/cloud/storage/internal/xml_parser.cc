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
#include "google/cloud/storage/internal/xml_parser_options.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::shared_ptr<XmlNode>> XmlParser::Parse(std::string const& content,
                                                    Options options) {
  internal::CheckExpectedOptions<XmlParserOptionsList>(options, __func__);
  options = XmlParserDefaultOptions(std::move(options));

  // Check size first
  if (content.size() > options.get<XmlParserMaxSourceSize>()) {
    return internal::InvalidArgumentError(
        absl::StrCat("The source size ", content.size(),
                     " exceeds the max size of ",
                     options.get<XmlParserMaxSourceSize>()),
        GCP_ERROR_INFO());
  }
  // Remove unnecessary bits
  auto trimmed = std::regex_replace(content, unneeded_re_, "");
  // And to be continued...
  return internal::UnimplementedError("not implemented");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
