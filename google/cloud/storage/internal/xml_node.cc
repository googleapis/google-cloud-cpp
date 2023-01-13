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

#include "google/cloud/storage/internal/xml_node.h"
#include "google/cloud/storage/internal/xml_escape.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <stack>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string XmlNode::GetConcatenatedText() const {
  // For non-tag element, just returns the text content.
  if (!text_content_.empty()) return text_content_;

  std::string ret;
  std::stack<std::shared_ptr<XmlNode const>> stack;
  stack.push(shared_from_this());
  while (!stack.empty()) {
    auto const cur = stack.top();
    stack.pop();
    ret += cur->text_content_;
    // put them into the stack in reverse order
    for (auto it = cur->children_.rbegin(); it != cur->children_.rend(); ++it) {
      stack.push(*it);
    }
  }
  return ret;
}

std::vector<std::shared_ptr<XmlNode const>> XmlNode::GetChildren() const {
  return {children_.begin(), children_.end()};
}

std::vector<std::shared_ptr<XmlNode const>> XmlNode::GetChildren(
    std::string const& tag_name) const {
  std::vector<std::shared_ptr<XmlNode const>> ret;
  for (auto const& child : children_) {
    if (child->tag_name_ == tag_name) ret.push_back(child);
  }
  return ret;
}

std::string XmlNode::ToString(int indent_width,  // NOLINT(misc-no-recursion)
                              int indent_level) const {
  auto const separator = std::string(indent_width == 0 ? "" : "\n");
  auto const indentation = std::string(indent_width * indent_level, ' ');
  if (!tag_name_.empty()) ++indent_level;

  auto ret = [&] {
    if (!tag_name_.empty()) {
      return absl::StrCat(indentation, "<", EscapeXmlTag(tag_name_), ">",
                          separator);
    }
    if (!text_content_.empty()) {
      return absl::StrCat(indentation, EscapeXmlContent(text_content_),
                          separator);
    }
    return std::string{};
  }();

  for (auto const& child : children_) {
    absl::StrAppend(&ret, child->ToString(indent_width, indent_level));
  }
  if (!tag_name_.empty()) {
    absl::StrAppend(&ret, indentation, "</", EscapeXmlTag(tag_name_), ">",
                    separator);
  }
  return ret;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
