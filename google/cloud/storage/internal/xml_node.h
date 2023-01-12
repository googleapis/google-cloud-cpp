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
 * @note This is used in the implementation of [GCS MPU]
 *
 * [GCS MPU]: https://cloud.google.com/storage/docs/multipart-uploads
 *
 * Normally a single node represents an XML element(tag), but we also
 * treat a text portion as a node. If the tag_name_ is empty, it's considered as
 * a text node.
 *
 * This is not a general purpose XML node. It is only intended to support XML
 * trees as used in the [GCS MPU]. It does not support many XML features.
 */
class XmlNode : public std::enable_shared_from_this<XmlNode> {
 public:
  XmlNode() = default;
  explicit XmlNode(std::string tag_name, std::string text_content)
      : tag_name_(std::move(tag_name)),
        text_content_(std::move(text_content)){};
  ~XmlNode() = default;

  /// Get the tag name.
  std::string GetTagName() const { return tag_name_; };
  /// Set the tag name.
  void SetTagName(std::string tag_name) { tag_name_ = std::move(tag_name); };

  /// Get the text content.
  std::string GetTextContent() const { return text_content_; };
  /// Set the text content.
  void SetTextContent(std::string text_content) {
    text_content_ = std::move(text_content);
  }
  /// Get the concatenated text content within the tag.
  std::string GetConcatenatedText() const;

  /// Get all the direct children.
  std::vector<std::shared_ptr<XmlNode const>> GetChildren() const;
  /// Get all the direct children that match the given tag name.
  std::vector<std::shared_ptr<XmlNode const>> GetChildren(
      std::string const& tag_name) const;

  /// Get the XML string representation of the node.
  std::string ToString(int indent_width = 0, int indent_level = 0) const;

  /// Append a new child and return the pointer to the added node.
  template <typename... Args>
  std::shared_ptr<XmlNode> AppendChild(Args... args) {
    children_.emplace_back(
        std::make_shared<XmlNode>(std::forward<Args>(args)...));
    return children_.back();
  }

 private:
  std::string tag_name_;
  std::string text_content_;
  std::vector<std::shared_ptr<XmlNode>> children_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_NODE_H
