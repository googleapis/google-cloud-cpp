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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_UTILS_H

#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <map>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Xml parse/builder library for GCS MPU upload:
 * https://cloud.google.com/storage/docs/multipart-uploads
 */

/**
 * Represents an XML node in a XML tree.
 *
 * Normally a single node represents an XML element (tag), but we also
 * treat a text portion as a node. If the tag_name_ is empty, it's considered as
 * a text node.
 *
 * For our use case, we don't need XML attributes at all.
 **/
class XmlNode {
 public:
  XmlNode() = default;
  explicit XmlNode(std::string tag_name, std::string text_content)
      : tag_name_(std::move(tag_name)),
        text_content_(std::move(text_content)){};
  ~XmlNode() = default;
  // Non copyable
  XmlNode(XmlNode const&) = delete;
  XmlNode& operator=(XmlNode&) = delete;
  // Movable
  XmlNode(XmlNode&& other) = default;
  XmlNode& operator=(XmlNode&& rhs) = default;
  /// Get the tag name.
  std::string GetTagName() const { return tag_name_; };
  /// Get the text content.
  std::string GetTextContent() const { return text_content_; };
  /// Set the tag name.
  void SetTagName(std::string tag_name);
  /// Set the text content.
  void SetTextContent(std::string text_content);
  /// Get all the direct children.
  std::vector<XmlNode const*> GetChildren() const;
  /// Get the first child that matches the given tag name.
  StatusOr<XmlNode const*> GetChild(std::string const& tag_name) const;
  /// Get all the direct children that match the given tag name.
  std::vector<XmlNode const*> GetChildren(std::string const& tag_name) const;
  /// Get the concatenated text content within the tag.
  std::string GetConcatenatedText() const;
  /// Get the XML string representation of the node.
  std::string ToString(int indent_size = 0, int indent = 0) const;
  /// Add a new child at the back of the `children_` vector.
  template <typename... Args>
  XmlNode* EmplaceChild(Args... args);

 private:
  std::string tag_name_;
  std::string text_content_;
  std::vector<XmlNode> children_;
};

template <typename... Args>
XmlNode* XmlNode::EmplaceChild(Args... args) {
  children_.emplace_back(std::forward<Args>(args)...);
  return &children_.back();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_UTILS_H
