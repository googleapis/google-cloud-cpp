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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "absl/strings/str_format.h"
#include <stack>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {

std::string EscapeXmlString(std::string const& val, bool for_text) {
  auto ret =
      absl::StrReplaceAll(val, {{"&", "&amp;"}, {"<", "&lt;"}, {">", "&gt;"}});
  if (!for_text) {
    ret = absl::StrReplaceAll(ret, {{"\"", "&quot;"}, {"'", "&apos;"}});
  }
  return ret;
}

}

std::string XmlNode::GetConcatenatedText() const {
  if (!text_content_.empty()) {
    // For non-tag element, just returns the text content.
    return text_content_;
  }
  std::string ret;
  std::stack<XmlNode const*> stack;
  stack.push(this);
  while (!stack.empty()) {
    auto const* cur = stack.top();
    stack.pop();
    ret += cur->text_content_;
    // put them into the stack in reverse order
    for (auto it = cur->children_.rbegin(); it != cur->children_.rend(); ++it) {
      stack.push(&*it);
    }
  }
  return ret;
}

std::vector<XmlNode const*> XmlNode::GetChildren() const {
  std::vector<XmlNode const*> ret;
  for (auto const& child : children_) {
    ret.push_back(&child);
  }
  return ret;
}

std::vector<XmlNode const*> XmlNode::GetChildren(
    std::string const& tag_name) const {
  std::vector<XmlNode const*> ret;
  for (auto const& child : children_) {
    if (child.tag_name_ == tag_name) {
      ret.push_back(&child);
    }
  }
  return ret;
}

std::string XmlNode::ToString(int indent_width,  // NOLINT(misc-no-recursion)
                              int indent_level) const {
  std::string ret;

  std::string separator = indent_width == 0 ? "" : "\n";
  int next_indent = indent_level;

  std::string const indentation(indent_width * indent_level, ' ');

  if (!tag_name_.empty()) {
    absl::StrAppendFormat(
        &ret, "%s<%s>%s",
        indentation,EscapeXmlString(tag_name_, false), separator);
    next_indent++;
  } else if (!text_content_.empty()) {
    absl::StrAppend(&ret, indentation, EscapeXmlString(text_content_, true),
                    separator);
  }
  for (auto const& child : children_) {
    absl::StrAppend(&ret, child.ToString(indent_width, next_indent));
  }
  if (!tag_name_.empty()) {
    absl::StrAppendFormat(
        &ret, "%s</%s>%s",
        indentation, EscapeXmlString(tag_name_, false), separator);
  }
  return ret;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
