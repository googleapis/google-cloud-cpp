// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/http_annotation_parser.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/match.h"

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

template <typename T>
struct ParseSuccess {
  T value;
  std::size_t end;  // where parsing finished
};
template <typename T>
ParseSuccess<T> MakeParseSuccess(T value, std::size_t end) {
  return ParseSuccess<T>{std::move(value), end};
}

template <typename T>
using ParseResult = StatusOr<ParseSuccess<T>>;

Status ParseError(absl::string_view input, std::size_t offset,
                  absl::string_view expected,
                  internal::ErrorInfoBuilder builder) {
  return internal::InvalidArgumentError(
      absl::StrCat("error parsing path template, expected", expected,
                   " at offset ", offset, "\n", input, "\n",
                   std::string(offset, ' '), "^"),
      std::move(builder));
}

ParseResult<PathTemplate::Segments> ParseSegments(absl::string_view input,
                                                  std::size_t offset);

ParseResult<PathTemplate::Segments> ParsePlainSegments(absl::string_view input,
                                                       std::size_t offset);

ParseResult<PathTemplate::Segment> ParsePlainSegment(absl::string_view input,
                                                     std::size_t offset);

ParseResult<PathTemplate::Segment> ParseSegment(absl::string_view input,
                                                std::size_t offset);

ParseResult<PathTemplate::Segment> ParseVariable(absl::string_view input,
                                                 std::size_t offset);

ParseResult<PathTemplate::Segments> ParsePlainSegments(absl::string_view input,
                                                       std::size_t offset) {
  PathTemplate::Segments segments;
  while (input.size() != offset) {
    auto s = ParsePlainSegment(input, offset);
    if (!s) return std::move(s).status();
    segments.push_back(
        std::make_shared<PathTemplate::Segment>(std::move(s->value)));
    offset = s->end;
    if (input.size() == offset || input[offset] != '/') break;
    ++offset;
  }
  if (segments.empty()) {
    return ParseError(input, offset, " segment", GCP_ERROR_INFO());
  }
  return MakeParseSuccess(std::move(segments), offset);
}

ParseResult<PathTemplate::Segments> ParseSegments(absl::string_view input,
                                                  std::size_t offset) {
  PathTemplate::Segments segments;
  while (input.size() != offset) {
    auto s = ParseSegment(input, offset);
    if (!s) return std::move(s).status();
    segments.push_back(
        std::make_shared<PathTemplate::Segment>(std::move(s->value)));
    offset = s->end;
    if (input.size() == offset || input[offset] != '/') break;
    ++offset;
  }
  if (segments.empty()) {
    return ParseError(input, offset, " segment", GCP_ERROR_INFO());
  }
  return MakeParseSuccess(std::move(segments), offset);
}

ParseResult<PathTemplate::Segment> ParseLiteral(absl::string_view input,
                                                std::size_t offset) {
  auto constexpr kValidChars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghiljkmnopqrstuvwxyz"
      "0123456789"
      "-._~"
      "%"  // We do not validate percent encoding
      "!$&'()*+,;=";
  auto candidate = input.substr(offset);
  auto pos = candidate.find_first_not_of(kValidChars);
  auto literal = candidate.substr(0, pos);
  auto const end = pos == absl::string_view::npos ? input.size() : offset + pos;
  if (end == offset) {
    return ParseError(input, offset, " non-empty literal", GCP_ERROR_INFO());
  }
  return MakeParseSuccess(PathTemplate::Segment{std::string(literal)}, end);
}

ParseResult<PathTemplate::Segment> ParseSegment(absl::string_view input,
                                                std::size_t offset) {
  auto candidate = input.substr(offset);
  if (absl::StartsWith(candidate, "**")) {
    return MakeParseSuccess(
        PathTemplate::Segment{PathTemplate::MatchRecursive{}}, offset + 2);
  }
  if (absl::StartsWith(candidate, "*")) {
    return MakeParseSuccess(PathTemplate::Segment{PathTemplate::Match{}},
                            offset + 1);
  }
  if (absl::StartsWith(candidate, "{")) return ParseVariable(input, offset);
  return ParseLiteral(input, offset);
}

ParseResult<PathTemplate::Segment> ParsePlainSegment(absl::string_view input,
                                                     std::size_t offset) {
  auto candidate = input.substr(offset);
  if (absl::StartsWith(candidate, "**")) {
    return MakeParseSuccess(
        PathTemplate::Segment{PathTemplate::MatchRecursive{}}, offset + 2);
  }
  if (absl::StartsWith(candidate, "*")) {
    return MakeParseSuccess(PathTemplate::Segment{PathTemplate::Match{}},
                            offset + 1);
  }
  if (absl::StartsWith(candidate, "{")) {
    return ParseError(input, offset, " literal", GCP_ERROR_INFO());
  }
  return ParseLiteral(input, offset);
}

ParseResult<std::string> ParseIdent(absl::string_view input,
                                    std::size_t offset) {
  auto candidate = input.substr(offset);
  auto constexpr kValidStartChars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghiljkmnopqrstuvwxyz";
  if (candidate.find_first_of(kValidStartChars) != 0) {
    return ParseError(input, offset, " start of identifier", GCP_ERROR_INFO());
  }
  auto constexpr kValidChars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghiljkmnopqrstuvwxyz"
      "0123456789"
      "_";
  auto pos = candidate.find_first_not_of(kValidChars);
  auto ident = candidate.substr(0, pos);
  auto const end = pos == absl::string_view::npos ? input.size() : offset + pos;
  return MakeParseSuccess(std::string(ident), end);
}

ParseResult<std::string> ParseFieldPath(absl::string_view input,
                                        std::size_t offset) {
  std::string field_path;
  while (input.size() != offset) {
    auto s = ParseIdent(input, offset);
    if (!s) return std::move(s).status();
    field_path.append(s->value);
    offset = s->end;
    if (input.size() == offset || input[offset] != '.') break;
    ++offset;
  }
  if (field_path.empty()) {
    return ParseError(input, offset, " identifier", GCP_ERROR_INFO());
  }
  return MakeParseSuccess(std::move(field_path), offset);
}

ParseResult<PathTemplate::Segment> ParseVariable(absl::string_view input,
                                                 std::size_t offset) {
  auto candidate = input.substr(offset);
  if (candidate.empty() || candidate[0] != '{') {
    return ParseError(input, offset, " opening brace", GCP_ERROR_INFO());
  }
  auto fp = ParseFieldPath(input, offset + 1);
  if (!fp) return std::move(fp).status();
  auto result = PathTemplate::Variable{std::move(fp->value), {}};
  offset = fp->end;
  if (input.size() == offset) {
    return ParseError(input, offset, " closing brace", GCP_ERROR_INFO());
  }
  if (input[offset] == '}') {
    return MakeParseSuccess(PathTemplate::Segment{std::move(result)},
                            offset + 1);
  }
  if (input[offset] != '=') {
    return ParseError(input, offset, " `=` or `}`", GCP_ERROR_INFO());
  }
  auto ps = ParsePlainSegments(input, offset + 1);
  if (!ps) return std::move(ps).status();
  offset = ps->end;
  if (input.size() == offset || input[offset] != '}') {
    return ParseError(input, offset, " closing brace", GCP_ERROR_INFO());
  }
  result.segments = std::move(ps->value);
  return MakeParseSuccess(PathTemplate::Segment{std::move(result)}, offset + 1);
}

ParseResult<std::string> ParseVerb(absl::string_view input,
                                   std::size_t offset) {
  auto verb = input.substr(offset);
  if (verb == "post" || verb == "get" || verb == "patch" || verb == "delete" ||
      verb == "put") {
    return MakeParseSuccess(std::string(verb), input.size());
  }
  return ParseError(input, offset, " http verb", GCP_ERROR_INFO());
}

void StreamSegments(std::ostream& os, PathTemplate::Segments const& segments) {
  os << "[ ";
  char const* sep = "";
  for (auto const& s : segments) {
    os << sep << *s;
    sep = " / ";
  }
  os << " ]";
}

}  // namespace

StatusOr<PathTemplate> ParsePathTemplate(absl::string_view input) {
  if (input.empty() || input[0] != '/') {
    return ParseError(input, 0, " '/'", GCP_ERROR_INFO());
  }
  auto s = ParseSegments(input, 1);
  if (!s) return std::move(s).status();
  auto offset = s->end;
  if (input.size() == offset) return PathTemplate{std::move(s->value), {}};
  if (input[offset] != ':') {
    return ParseError(input, offset, " ':'", GCP_ERROR_INFO());
  }
  ++offset;
  auto v = ParseVerb(input, offset);
  if (!v) return std::move(v).status();
  offset = v->end;
  if (input.size() != offset) {
    return ParseError(input, v->end, " end of input", GCP_ERROR_INFO());
  }
  return PathTemplate{std::move(s->value), v->value};
}

std::ostream& operator<<(std::ostream& os, PathTemplate::Segment const& rhs) {
  struct Visitor {
    std::ostream& os;
    void operator()(PathTemplate::Match const&) { os << "*"; }
    void operator()(PathTemplate::MatchRecursive const&) { os << "**"; }
    void operator()(std::string const& s) { os << "segment=" << s; }
    void operator()(PathTemplate::Variable const& v) {
      os << v.field_path;
      if (v.field_path.empty()) return;
      StreamSegments(os, v.segments);
    }
  };
  os << "{";
  absl::visit(Visitor{os}, rhs.value);
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, PathTemplate const& rhs) {
  os << "{segments=";
  StreamSegments(os, rhs.segments);
  if (!rhs.verb.empty()) os << ", verb=" << rhs.verb;
  os << "}";
  return os;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
