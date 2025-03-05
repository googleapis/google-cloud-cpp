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

#include "generator/internal/resolve_comment_references.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include <regex>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

template <typename Descriptor>
absl::optional<std::pair<std::string, ProtoDefinitionLocation>> GetLocation(
    Descriptor const* d) {
  if (d == nullptr) return absl::nullopt;
  google::protobuf::SourceLocation loc;
  d->GetSourceLocation(&loc);
  return std::make_pair(std::string{d->full_name()},
                        ProtoDefinitionLocation{std::string{d->file()->name()},
                                                loc.start_line + 1});
}

absl::optional<std::pair<std::string, ProtoDefinitionLocation>>
FindByAlternativeEnumValueName(google::protobuf::DescriptorPool const& pool,
                               std::string const& name) {
  std::vector<absl::string_view> components = absl::StrSplit(name, '.');
  if (components.size() < 2) return absl::nullopt;
  components.erase(std::next(components.begin(), components.size() - 2));
  auto alternative = absl::StrJoin(components, ".");
  auto result = GetLocation(pool.FindEnumValueByName(alternative));
  if (!result.has_value()) return result;
  return std::make_pair(name, std::move(result->second));
}

/// Search @p pool for an entity called @p name and return its fully qualified
/// name and location.
absl::optional<std::pair<std::string, ProtoDefinitionLocation>> FindByName(
    google::protobuf::DescriptorPool const& pool, std::string const& name) {
  auto location = GetLocation(pool.FindEnumTypeByName(name));
  if (location.has_value()) return location;
  location = GetLocation(pool.FindEnumValueByName(name));
  if (location.has_value()) return location;
  location = GetLocation(pool.FindExtensionByName(name));
  if (location.has_value()) return location;
  location = GetLocation(pool.FindFieldByName(name));
  if (location.has_value()) return location;
  location = GetLocation(pool.FindMessageTypeByName(name));
  if (location.has_value()) return location;
  location = GetLocation(pool.FindMethodByName(name));
  if (location.has_value()) return location;
  location = GetLocation(pool.FindOneofByName(name));
  if (location.has_value()) return location;
  location = GetLocation(pool.FindServiceByName(name));
  if (location.has_value()) return location;

  // Last ditch, sometimes the comments use `foo.bar.Baz.EnumName.EnumValue`.
  // The Protobuf library can only find `foo.bar.Baz.EnumValue`.
  return FindByAlternativeEnumValueName(pool, name);
}

}  // namespace

std::map<std::string, ProtoDefinitionLocation> ResolveCommentReferences(
    std::string const& comment, google::protobuf::DescriptorPool const& pool) {
  auto const re = std::regex(R"re(\]\[([a-z_]+\.[a-zA-Z0-9_\.]+)\])re");
  auto begin = std::sregex_iterator(comment.begin(), comment.end(), re);
  std::map<std::string, ProtoDefinitionLocation> references;
  for (auto i = begin; i != std::sregex_iterator{}; ++i) {
    auto const& match = *i;
    if (match.size() != 2) continue;
    auto location = FindByName(pool, match[1].str());
    if (location) references.insert(std::move(*location));
  }
  return references;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
