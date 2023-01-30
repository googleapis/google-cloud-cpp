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
#include "google/cloud/storage/internal/xml_parser_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/ascii.h"
#include <iterator>
#include <regex>
#include <stack>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

bool IsSpace(char c) {
  return absl::ascii_isspace(static_cast<unsigned char>(c));
}

std::string StripTrailingSpaces(absl::string_view str) {
  return std::string(absl::StripTrailingAsciiWhitespace(str));
}

// Should skip these during the parse instead, to avoid copying the document.
std::string StripNonessential(absl::string_view document) {
  static auto* nonessential_re = new std::regex{
      "("
      R"(<!DOCTYPE[^>[]*(\[[^\]]*\])?>)"  // DTD(DOCTYPE)
      "|"
      R"(<!\[CDATA\[[\s\S]*?\]\]>)"  // CDATA
      "|"
      R"(<!--[\s\S]*?-->)"  // XML comments
      ")",
      std::regex::icase | std::regex::nosubs | std::regex::optimize};
  std::string essential;
  std::regex_replace(std::back_inserter(essential), document.begin(),
                     document.end(), *nonessential_re, "");
  return essential;
}

std::string EscapeTagName(absl::string_view tag_name) {
  return absl::StrReplaceAll(tag_name, {{"&", "&amp;"},
                                        {"<", "&lt;"},
                                        {">", "&gt;"},
                                        {"\"", "&quot;"},
                                        {"'", "&apos;"}});
}

std::string UnescapeTagName(absl::string_view tag_name) {
  return absl::StrReplaceAll(tag_name, {{"&amp;", "&"},
                                        {"&lt;", "<"},
                                        {"&gt;", ">"},
                                        {"&quot;", "\""},
                                        {"&apos;", "'"}});
}

std::string EscapeTextContent(absl::string_view text_content) {
  return absl::StrReplaceAll(text_content,
                             {{"&", "&amp;"}, {"<", "&lt;"}, {">", "&gt;"}});
}

std::string UnescapeTextContent(absl::string_view text_content) {
  return absl::StrReplaceAll(text_content,
                             {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}});
}

// Consume a string_view rvalue, with an empty moved-from state.
absl::string_view Move(absl::string_view& s) {
  absl::string_view r;
  r.swap(s);
  return r;
}

// Extend a string_view by one char. The caller must know that this is
// a safe operation (i.e., the arg is part of a larger string object).
absl::string_view Extend(absl::string_view s) {
  return absl::string_view(s.data(), s.size() + 1);
}

class XmlParser {
 public:
  explicit XmlParser(absl::string_view document, Options const& options)
      : document_(StripNonessential(document)),
        max_node_count_(options.get<XmlParserMaxNodeCount>()),
        max_node_depth_(options.get<XmlParserMaxNodeDepth>()),
        unparsed_(document_) {}

  StatusOr<std::shared_ptr<XmlNode>> Parse();

 private:
  Status SkipDeclaration();

  // Parsing-state handlers.
  Status HandleBase();
  Status HandleStartTag();
  Status HandleReadingTag();
  Status HandleReadingAttr();
  Status HandleEndTag();
  Status HandleReadingText();
  Status HandleBeginClosingTag();
  Status HandleReadingClosingTag();

  Status CheckLimits();
  StatusOr<std::shared_ptr<XmlNode>> AppendTagNode(std::string tag_name);
  StatusOr<std::shared_ptr<XmlNode>> AppendTextNode(std::string text_content);

  std::string document_;
  std::size_t max_node_count_;
  std::size_t max_node_depth_;

  absl::string_view unparsed_;  // suffix of document_
  Status (XmlParser::*handler_)() = &XmlParser::HandleBase;
  std::stack<std::shared_ptr<XmlNode>> node_stack_;  // root + tag*
  std::size_t node_count_ = 0;
  absl::string_view tag_name_;
  absl::string_view text_content_;
  absl::string_view end_tag_;
};

StatusOr<std::shared_ptr<XmlNode>> XmlParser::Parse() {
  auto res = SkipDeclaration();
  if (!res.ok()) return res;

  node_stack_.push(XmlNode::CreateRoot());
  while (!unparsed_.empty()) {
    auto result = (this->*handler_)();
    if (!result.ok()) return result;
    unparsed_.remove_prefix(1);
  }

  auto top = std::move(node_stack_.top());
  node_stack_.pop();
  if (!node_stack_.empty()) {
    return internal::InvalidArgumentError(
        absl::StrCat("Unterminated tag '", top->GetTagName(), "'"),
        GCP_ERROR_INFO());
  }
  return top;
}

// Should skip this during the parse instead.
Status XmlParser::SkipDeclaration() {
  constexpr static absl::string_view kDeclStart = "<?xml";
  constexpr static absl::string_view kDeclEnd = "?>";
  // Note: This also skips anything before the declaration.
  auto decl_start = unparsed_.find(kDeclStart);
  if (decl_start != std::string::npos) {
    auto decl_end = unparsed_.find(kDeclEnd, decl_start + kDeclStart.size());
    if (decl_end == std::string::npos) {
      return internal::InvalidArgumentError("Unterminated XML declaration",
                                            GCP_ERROR_INFO());
    }
    unparsed_.remove_prefix(decl_end + kDeclEnd.size());
  }
  return Status();
}

Status XmlParser::HandleBase() {
  auto c = unparsed_.front();
  if (!IsSpace(c)) {
    if (c != '<') {
      return internal::InvalidArgumentError(
          absl::StrCat("Expected tag but found '", unparsed_.substr(0, 4), "'"),
          GCP_ERROR_INFO());
    }
    handler_ = &XmlParser::HandleStartTag;
  }
  return Status();
}

Status XmlParser::HandleStartTag() {
  auto c = unparsed_.front();
  if (c == '/') {
    handler_ = &XmlParser::HandleBeginClosingTag;
  } else if (!IsSpace(c)) {
    tag_name_ = unparsed_.substr(0, 1);
    handler_ = &XmlParser::HandleReadingTag;
  }
  return Status();
}

Status XmlParser::HandleReadingTag() {
  auto c = unparsed_.front();
  if (IsSpace(c)) {
    handler_ = &XmlParser::HandleReadingAttr;
  } else if (c == '>') {
    // The tag ends, so append a new tag node and push it onto the stack,
    // increasing both the node count and the path depth.
    auto tag_node = AppendTagNode(UnescapeTagName(Move(tag_name_)));
    if (!tag_node) return std::move(tag_node).status();
    node_stack_.push(std::move(*tag_node));
    handler_ = &XmlParser::HandleEndTag;
  } else if (c == '/') {
    // This is a tag with this form <TAG/>. Read ahead to the next '>'.
    auto close_tag_pos = unparsed_.find('>', 1);
    if (close_tag_pos == std::string::npos) {
      return internal::InvalidArgumentError("The tag never closes",
                                            GCP_ERROR_INFO());
    }
    // Note: This is the only place a handler consumes additional input.
    // We should probably deal with that using the state machine instead,
    // or use the same tactic in other handlers too.
    unparsed_.remove_prefix(close_tag_pos);
    auto tag_node = AppendTagNode(UnescapeTagName(Move(tag_name_)));
    if (!tag_node) return std::move(tag_node).status();
    // We optimize away the node_stack_ push/pop of tag_node, but we've
    // still performed the max_node_depth_ check as if we had pushed.
    handler_ = &XmlParser::HandleBase;
  } else {
    tag_name_ = Extend(tag_name_);
  }
  return Status();
}

// We don't need the attributes at all, so ignore them.
Status XmlParser::HandleReadingAttr() {
  auto c = unparsed_.front();
  if (c == '>') {
    auto tag_node = AppendTagNode(UnescapeTagName(Move(tag_name_)));
    if (!tag_node) return std::move(tag_node).status();
    node_stack_.push(std::move(*tag_node));
    handler_ = &XmlParser::HandleEndTag;
  }
  return Status();
}

Status XmlParser::HandleEndTag() {
  auto c = unparsed_.front();
  if (!IsSpace(c)) {  // Left trim text content.
    if (c == '<') {
      handler_ = &XmlParser::HandleStartTag;
    } else {
      // A text part starts.
      text_content_ = unparsed_.substr(0, 1);
      handler_ = &XmlParser::HandleReadingText;
    }
  }
  return Status();
}

Status XmlParser::HandleReadingText() {
  auto c = unparsed_.front();
  if (c == '<') {
    // Add a text node to the prevailing tag node if the limits allow.
    auto text_node = AppendTextNode(
        UnescapeTextContent(StripTrailingSpaces(Move(text_content_))));
    if (!text_node) return std::move(text_node).status();
    handler_ = &XmlParser::HandleStartTag;
  } else {
    text_content_ = Extend(text_content_);
  }
  return Status();
}

Status XmlParser::HandleBeginClosingTag() {
  auto c = unparsed_.front();
  if (!IsSpace(c)) {  // Left trim tag names.
    end_tag_ = unparsed_.substr(0, 1);
    handler_ = &XmlParser::HandleReadingClosingTag;
    if (c == '>') {
      // "</>" is invalid.
      return internal::InvalidArgumentError("Invalid tag '</>' found",
                                            GCP_ERROR_INFO());
    }
  }
  return Status();
}

Status XmlParser::HandleReadingClosingTag() {
  auto c = unparsed_.front();
  if (!IsSpace(c)) {  // Left trim tag names.
    if (c == '>') {
      auto start_tag = node_stack_.top()->GetTagName();
      auto end_tag = UnescapeTagName(StripTrailingSpaces(Move(end_tag_)));
      if (end_tag != start_tag) {
        return internal::InvalidArgumentError(
            absl::StrCat("Mismatched end tag: found '", end_tag,
                         "', but expected '", start_tag, "'"),
            GCP_ERROR_INFO());
      }
      node_stack_.pop();  // The current tag ends.
      handler_ = &XmlParser::HandleBase;
    } else {
      end_tag_ = Extend(end_tag_);
    }
  }
  return Status();
}

Status XmlParser::CheckLimits() {
  if (node_count_ == max_node_count_) {
    return internal::InvalidArgumentError(
        absl::StrCat("Exceeds max node count of ", max_node_count_),
        GCP_ERROR_INFO());
  }
  if (node_stack_.size() == max_node_depth_) {
    return internal::InvalidArgumentError(
        absl::StrCat("Exceeds max node depth of ", max_node_depth_),
        GCP_ERROR_INFO());
  }
  return Status();
}

StatusOr<std::shared_ptr<XmlNode>> XmlParser::AppendTagNode(
    std::string tag_name) {
  auto res = CheckLimits();
  if (!res.ok()) return res;
  auto node = node_stack_.top()->AppendTagNode(std::move(tag_name));
  ++node_count_;
  return node;
}

StatusOr<std::shared_ptr<XmlNode>> XmlParser::AppendTextNode(
    std::string text_content) {
  auto res = CheckLimits();
  if (!res.ok()) return res;
  auto node = node_stack_.top()->AppendTextNode(std::move(text_content));
  ++node_count_;
  return node;
}

}  // namespace

StatusOr<std::shared_ptr<XmlNode>> XmlNode::Parse(absl::string_view document,
                                                  Options options) {
  internal::CheckExpectedOptions<XmlParserOptionsList>(options, __func__);
  options = XmlParserDefaultOptions(std::move(options));

  // Check size first.
  auto max_source_size = options.get<XmlParserMaxSourceSize>();
  if (document.size() > max_source_size) {
    return internal::InvalidArgumentError(
        absl::StrCat("The source size ", document.size(),
                     " exceeds the max size of ", max_source_size),
        GCP_ERROR_INFO());
  }

  return XmlParser(document, options).Parse();
}

std::shared_ptr<XmlNode> XmlNode::CompleteMultipartUpload(
    std::map<std::size_t, std::string> const& parts) {
  auto root = CreateRoot();
  auto target_node = root->AppendTagNode("CompleteMultipartUpload");
  for (auto const& p : parts) {
    auto part_tag = target_node->AppendTagNode("Part");
    part_tag->AppendTagNode("PartNumber")
        ->AppendTextNode(std::to_string(p.first));
    part_tag->AppendTagNode("ETag")->AppendTextNode(
        EscapeTextContent(p.second));
  }
  return root;
}

std::string XmlNode::GetConcatenatedText() const {
  // For non-tag element, just returns the text content.
  if (!text_content_.empty()) return text_content_;

  std::string text;
  std::stack<std::shared_ptr<XmlNode const>> stack;
  stack.push(shared_from_this());
  while (!stack.empty()) {
    auto const cur = stack.top();
    stack.pop();
    text += cur->text_content_;
    // Push onto the stack in reverse order.
    for (auto it = cur->children_.rbegin(); it != cur->children_.rend(); ++it) {
      stack.push(*it);
    }
  }
  return text;
}

std::vector<std::shared_ptr<XmlNode const>> XmlNode::GetChildren() const {
  return {children_.begin(), children_.end()};
}

std::vector<std::shared_ptr<XmlNode const>> XmlNode::GetChildren(
    std::string const& tag_name) const {
  std::vector<std::shared_ptr<XmlNode const>> children;
  for (auto const& child : children_) {
    if (child->tag_name_ == tag_name) children.push_back(child);
  }
  return children;
}

std::string XmlNode::ToString(int indent_width,  // NOLINT(misc-no-recursion)
                              int indent_level) const {
  auto const separator = std::string(indent_width == 0 ? "" : "\n");
  auto const indentation = std::string(indent_width * indent_level, ' ');
  if (!tag_name_.empty()) ++indent_level;

  auto str = [&] {
    if (!tag_name_.empty()) {
      return absl::StrCat(indentation, "<", EscapeTagName(tag_name_), ">",
                          separator);
    }
    if (!text_content_.empty()) {
      return absl::StrCat(indentation, EscapeTextContent(text_content_),
                          separator);
    }
    return std::string{};
  }();

  for (auto const& child : children_) {
    absl::StrAppend(&str, child->ToString(indent_width, indent_level));
  }
  if (!tag_name_.empty()) {
    absl::StrAppend(&str, indentation, "</", EscapeTagName(tag_name_), ">",
                    separator);
  }
  return str;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
