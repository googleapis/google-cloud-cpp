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

#include "google/cloud/storage/internal/xml_utils.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "absl/strings/str_format.h"
#include <absl/strings/str_replace.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {  // Anonymous namespace

std::string EscapeXmlString(std::string const& val, bool for_text) {
  auto ret =
      absl::StrReplaceAll(val, {{"&", "&amp;"}, {"<", "&lt;"}, {">", "&gt;"}});
  if (!for_text) {
    ret = absl::StrReplaceAll(ret, {{"\"", "&quot;"}, {"'", "&apos;"}});
  }
  return ret;
}

}  // Anonymous namespace

StatusOr<XmlNode const*> XmlNode::GetChild(std::string const& tag_name) const {
  for (auto const& child : children_) {
    if (child.tag_name_ == tag_name) {
      return &child;
    }
  }
  return Status(StatusCode::kNotFound,
                absl::StrFormat("Tag '%s' not found", tag_name));
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

std::vector<XmlNode const*> XmlNode::GetChildren() const {
  std::vector<XmlNode const*> ret;
  for (auto const& child : children_) {
    ret.push_back(&child);
  }
  return ret;
}

std::string XmlNode::GetConcatenatedText() const {
  std::string ret;
  std::vector<XmlNode const*> stack;
  stack.push_back(this);
  while (!stack.empty()) {
    auto const* cur = stack.back();
    stack.pop_back();
    ret += cur->text_content_;
    // put them into the stack in reverse order
    for (auto it = cur->children_.rbegin(); it != cur->children_.rend(); it++) {
      stack.push_back(&*it);
    }
  }
  return ret;
}

std::string XmlNode::ToString(int indent_size,  // NOLINT(misc-no-recursion)
                              int indent) const {
  std::string ret;
  std::string indentation;
  std::string newline = indent_size == 0 ? "" : "\n";
  int next_indent = indent;
  for (auto i = 0; i < indent * indent_size; i++) {
    absl::StrAppend(&indentation, " ");
  }
  if (!tag_name_.empty()) {
    absl::StrAppend(
        &ret, absl::StrFormat("%s<%s>%s", indentation,
                              EscapeXmlString(tag_name_, false), newline));
    next_indent++;
  } else if (!text_content_.empty()) {
    absl::StrAppend(&ret, indentation, EscapeXmlString(text_content_, true),
                    newline);
  }
  for (auto const& child : children_) {
    absl::StrAppend(&ret, child.ToString(indent_size, next_indent));
  }
  if (!tag_name_.empty()) {
    absl::StrAppend(
        &ret, absl::StrFormat("%s</%s>%s", indentation,
                              EscapeXmlString(tag_name_, false), newline));
  }
  return ret;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
