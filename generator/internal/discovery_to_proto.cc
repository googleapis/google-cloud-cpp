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

#include "generator/internal/discovery_to_proto.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/status_or.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

namespace rest = google::cloud::rest_internal;

google::cloud::StatusOr<std::string> GetPage(std::string const& url) {
  std::pair<std::string, std::string> url_pieces =
      absl::StrSplit(url, absl::ByString("com/"));
  auto client = rest::MakeDefaultRestClient(url_pieces.first + "com", {});
  rest::RestRequest request;
  request.SetPath(url_pieces.second);
  auto response = client->Get(request);
  if (!response.ok()) {
    return response.status();
  }

  auto payload = std::move(**response).ExtractPayload();
  return rest::ReadAll(std::move(payload));
}

StatusOr<nlohmann::json> GetDiscoveryDoc(std::string const& url) {
  nlohmann::json parsed_discovery_doc;
  if (absl::StartsWith(url, "file://")) {
    auto file_path = std::string(absl::StripPrefix(url, "file://"));
    std::ifstream json_file(file_path);
    if (!json_file.is_open()) {
      return internal::InvalidArgumentError(
          absl::StrCat("Unable to open file ", file_path));
    }
    parsed_discovery_doc = nlohmann::json::parse(json_file, nullptr, false);
  } else {
    auto page = GetPage(url);
    if (!page.ok()) return page.status();
    parsed_discovery_doc = nlohmann::json::parse(*page, nullptr, false);
  }

  if (parsed_discovery_doc.is_discarded() || parsed_discovery_doc.is_null()) {
    return internal::DataLossError("Error parsing Discovery Doc");
  }

  return parsed_discovery_doc;
}

}  // namespace

Status GenerateProtosFromDiscoveryDoc(std::string const& url,
                                      std::string const&, std::string const&,
                                      std::string const&) {
  auto discovery_doc = GetDiscoveryDoc(url);
  if (!discovery_doc.ok()) return discovery_doc.status();

  // TODO(10980): Finish implementing this function.
  return internal::UnimplementedError("Not implemented.");
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
