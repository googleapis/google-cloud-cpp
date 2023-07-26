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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_HTTP_ANNOTATION_PARSER_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_HTTP_ANNOTATION_PARSER_H

#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include <memory>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * The result of parsing a `google.api.http` annotation.
 *
 * A `google.api.http` annotation describes how to convert gRPC RPCs to HTTP
 * URLs. The description uses a "path template", showing what portions of the
 * URL path are replaced with values from the gRPC request message.
 *
 * These path templates follow a specific grammar. The grammar is defined by:
 *
 *     Template = "/" Segments [ Verb ] ;
 *     Segments = Segment { "/" Segment } ;
 *     Segment  = "*" | "**" | LITERAL | Variable ;
 *     Variable = "{" FieldPath [ "=" Segments ] "}" ;
 *     FieldPath = IDENT { "." IDENT } ;
 *     Verb     = ":" LITERAL ;
 *
 * The specific notation is not defined, but it seems inspired by
 * [Backus-Naur Form].  In this notation, `{ ... }` allows repetition.
 *
 * The documentation goes on to say:
 *     A variable template must not contain other variables.
 * So the grammar is better defined by:
 *
 *     Template = "/" Segments [ Verb ] ;
 *     Segments = Segment { "/" Segment } ;
 *     Segment  = "*" | "**" | LITERAL | Variable ;
 *     PlainSegment  = "*" | "**" | LITERAL ;
 *     PlainSegments = PlainSegment { "/" PlainSegment };
 *     Variable = "{" FieldPath [ "=" PlainSegments ] "}" ;
 *     FieldPath = IDENT { "." IDENT } ;
 *     Verb     = ":" LITERAL ;
 *
 * Neither "IDENT" nor "LITERAL" are defined. From context we can infer that
 * IDENT must be a valid proto3 identifier, so matching the regular expression
 * `[A-Za-z][A-Za-z0-9_]*`. Likewise, we can infer that LITERAL must be path
 * segment in a URL. [RFC 3986] provides a definition for these, which we
 * summarize as [^1]:
 *
 * LITERAL     = pchar { pchar }
 * pchar       = unreserved | pct-encoded | sub-delims | ":" | "@"
 * unreserved  = ALPHA | DIGIT | "-" | "." | "_" | "~"
 * pct-encoded = "%" HEXDIG HEXDIG
 * sub-delims  = "!" | "$" | "&" | "'" | "(" | ")"
 *             | "*" | "+" | "," | ";" | "="
 * ALPHA       = [A-Za-z]
 * DIGIT       = [0-9]
 * HEXDIG      = [0-9A-Fa-f]
 *
 * [^1]: Using the notation from above, the RFC uses a different notation.
 * [RFC 3086]: https://datatracker.ietf.org/doc/html/rfc3986#section-3.3
 * [Backus-Naur Form]: https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form
 */
struct PathTemplate {
  struct Segment;
  struct Match {};           // Also known as '*'
  struct MatchRecursive {};  // Also known as '**'
  struct Variable {
    std::string field_path;
    std::vector<std::shared_ptr<Segment>> segments;
  };
  struct Segment {
    absl::variant<Match, MatchRecursive, std::string, Variable> value;
  };
  using Segments = std::vector<std::shared_ptr<Segment>>;

  Segments segments;
  std::string verb;  // possibly empty
};

/// Parses a google.api.http path template.
StatusOr<PathTemplate> ParsePathTemplate(absl::string_view input);

/// Streaming operator, used in testing and debugging.
std::ostream& operator<<(std::ostream& os, PathTemplate::Segment const& rhs);

/// Streaming operator, used in testing and debugging.
std::ostream& operator<<(std::ostream& os, PathTemplate const& rhs);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_HTTP_ANNOTATION_PARSER_H
