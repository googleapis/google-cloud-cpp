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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_NODE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_NODE_H

#include "google/cloud/storage/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Represents an XML node in an XML tree.
 *
 * Normally a single node represents an XML element(tag), but we also treat a
 * text portion as a node. If the tag_name_ is empty, it's considered a text
 * node.
 *
 * @note This is not a general purpose XML node. It is only intended to support
 * XML trees as used in the [GCS MPU]. It does not support many XML features.
 *
 * [GCS MPU]: https://cloud.google.com/storage/docs/multipart-uploads
 */
class XmlNode : public std::enable_shared_from_this<XmlNode> {
 public:
  ~XmlNode() = default;

  /// Create a root node.
  static std::shared_ptr<XmlNode> CreateRoot() {
    return std::shared_ptr<XmlNode>{new XmlNode};
  }

  /**
   * Parses the given XML document and returns an XML tree.
   *
   * As a defence to DOS type attacks, it has several limits. The default
   * values of these limits are large enough for API responses from [GCS MPU],
   * but in case you need to configure these limits, use the following
   * options: `XmlParserMaxSourceSize`, `XmlParserMaxNodeCount`, and
   * `XmlParserMaxNodeDepth`. For the default values of these limits, see
   * `google/cloud/storage/internal/xml_parser_options.h`.
   *
   * @note This is not a general purpose XML parser. It is only intended to
   * parse XML responses from the [GCS MPU]. It does not support many XML
   * features.
   */
  static StatusOr<std::shared_ptr<XmlNode>> Parse(absl::string_view document,
                                                  Options = {});

  /**
   * Creates an XML request for "Completing multipart upload" API described
   * at https://cloud.google.com/storage/docs/xml-api/post-object-complete.
   */
  static std::shared_ptr<XmlNode> CompleteMultipartUpload(
      std::map<std::size_t, std::string> const& parts);

  /// Get the tag name.
  std::string GetTagName() const { return tag_name_; };
  /// Get the text content.
  std::string GetTextContent() const { return text_content_; };

  /// Get the concatenated text content within the tag.
  std::string GetConcatenatedText() const;

  /// Get all the direct children.
  std::vector<std::shared_ptr<XmlNode const>> GetChildren() const;
  /// Get all the direct children that match the given tag name.
  std::vector<std::shared_ptr<XmlNode const>> GetChildren(
      std::string const& tag_name) const;

  /// Get the XML string representation of the node.
  std::string ToString(std::size_t indent_width = 0,
                       std::size_t indent_level = 0) const;

  /// Append a new tag node and return the added node.
  std::shared_ptr<XmlNode> AppendTagNode(std::string tag_name) {
    children_.emplace_back(new XmlNode(std::move(tag_name), ""));
    return children_.back();
  }
  /// Append a new text node and return the added node.
  std::shared_ptr<XmlNode> AppendTextNode(std::string text_content) {
    children_.emplace_back(new XmlNode("", std::move(text_content)));
    return children_.back();
  }

 private:
  XmlNode() = default;
  explicit XmlNode(std::string tag_name, std::string text_content)
      : tag_name_(std::move(tag_name)),
        text_content_(std::move(text_content)){};

  std::string tag_name_;
  std::string text_content_;
  std::vector<std::shared_ptr<XmlNode>> children_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_NODE_H
