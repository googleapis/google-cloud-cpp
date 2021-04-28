// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <google/cloud/functions/cloud_event.h>
#include <google/cloud/storage/client.h>
#include <cppcodec/base64_rfc4648.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <vector>

auto constexpr kGcsPrefix = "https://storage.googleapis.com/";
auto constexpr kPrPrefix =
    "https://github.com/googleapis/google-cloud-cpp/pull/";
auto constexpr kGCBPrefix =
    "https://console.cloud.google.com/cloud-build/builds?project=";
auto constexpr kAttempts = 4;

using ::google::cloud::StatusCode;
using ::google::cloud::functions::CloudEvent;
namespace gcs = ::google::cloud::storage;

namespace {

// Returns an HTML anchor referencing the given URL with the optional name. If
// `name` is not specified, the URL is used as the text.
std::string Anchor(std::string const& url, std::string name = "") {
  if (name.empty()) name = url;
  return "<a href=\"" + url + "\">" + name + "</a>";
}

// Writes an HTML table w/ the data from the vector.
void WriteTable(std::ostream& os,
                std::vector<std::vector<std::string>> const& table,
                std::vector<std::string> const& header = {}) {
  os << "<table>\n";
  if (!header.empty()) {
    os << "<tr>";
    for (auto const& col : header) {
      os << "<th>" << col << "</th>";
    }
    os << "</tr>\n";
  }
  for (auto const& row : table) {
    os << "<tr>";
    for (auto const& col : row) {
      os << "<td>" << col << "</td>";
    }
    os << "</tr>\n";
  }
  os << "</table>\n";
}

std::string const& bucket_name() {
  static auto const kBucketName = [] {
    auto const* bname = std::getenv("BUCKET_NAME");
    if (bname == nullptr) {
      throw std::runtime_error("BUCKET_NAME environment variable is required");
    }
    return std::string{bname};
  }();
  return kBucketName;
}

std::string const& destination() {
  static auto const kDestination = [] {
    auto const* destination = std::getenv("DESTINATION");
    return std::string{destination == nullptr ? "index.html" : destination};
  }();
  return kDestination;
}

auto HtmlHead(std::string const& pr, std::string const& sha) {
  std::ostringstream os;
  os << "<head><meta charset=\"utf-8\">";
  os << "<title>"
     << "PR #" << pr << " google-cloud-cpp@" << sha.substr(0, 7)
     << "</title>\n";
  os << "<style>\n";
  os << "tr:nth-child(even) {background: #FFF}\n";
  os << "tr:nth-child(odd) {background: #DDD}\n";
  os << "</style></head>";
  return os.str();
}

void LogsSummaryTable(std::ostream& os, gcs::Client client,
                      std::string const& prefix) {
  static auto const kLogfileRE =
      std::regex(R"re(/log-[0-9a-f-]+\.txt$)re", std::regex::optimize);
  std::vector<std::string> const header{std::string{"Build"},
                                        std::string{"Log"}};
  std::vector<std::vector<std::string>> table;
  for (auto const& object :
       client.ListObjects(bucket_name(), gcs::Prefix(prefix))) {
    if (!object) throw std::runtime_error(object.status().message());
    auto const path = std::filesystem::path(object->name());
    if (!std::regex_search(object->name(), kLogfileRE)) continue;
    table.emplace_back(std::vector<std::string>{
        path.parent_path().filename().string(),
        Anchor(kGcsPrefix + bucket_name() + "/" + object->name(), "raw log")});
  }
  WriteTable(os, table, header);
}

auto CreateContents(gcs::Client client, std::string const& prefix,
                    std::string const& html_head,
                    std::vector<std::vector<std::string>> const& preamble) {
  std::ostringstream os;
  os << "<!DOCTYPE html>\n";
  os << "<html>\n";
  os << html_head << "\n";
  os << "<body>\n";
  os << "<h1>Public Build Results</h1><hr/>\n";
  WriteTable(os, preamble);
  os << "<h2>Build logs</h2>\n";
  LogsSummaryTable(os, client, prefix);
  os << "<hr/><p>NOTE: To debug a build on your local machine use the ";
  os << "<code>ci/cloudbuild/build.sh</code> script, with its ";
  os << "<a "
        "href=\"https://github.com/googleapis/google-cloud-cpp/blob/master/"
        "ci/cloudbuild/build.sh\">documentation here</a>.";
  os << "</p>\n";
  os << "</body>\n";
  os << "</html>\n";
  return std::move(os).str();
}

nlohmann::json LogFormat(std::string const& sev, std::string const& msg) {
  return nlohmann::json{{"severity", sev}, {"message", msg}}.dump();
}

void LogError(std::string const& msg) {
  std::cerr << LogFormat("error", msg) << "\n";
}

void LogInfo(std::string const& msg) {
  std::cout << LogFormat("info", msg) << "\n";
}

}  // namespace

void IndexBuildLogs(CloudEvent event) {
  static auto client = [] {
    return gcs::Client::CreateDefaultClient().value();
  }();

  // We skip any events with these status as such builds do not have a final
  // log, and cannot affect the output of the index.html file.
  static auto const kSkippedStatus = std::unordered_set<std::string>{
      "STATUS_UNKNOWN", "QUEUED", "WORKING",
      "",  // empty status indicates an invalid message
  };

  if (event.data_content_type().value_or("") != "application/json") {
    return LogError("expected application/json data");
  }
  auto const payload = nlohmann::json::parse(event.data().value_or("{}"));
  if (payload.count("message") == 0) {
    return LogError("missing embedded Pub/Sub message");
  }
  auto const message = payload["message"];
  if (message.count("attributes") == 0 or message.count("data") == 0) {
    return LogError("missing Pub/Sub attributes or data");
  }
  auto const data = cppcodec::base64_rfc4648::decode<std::string>(
      message["data"].get<std::string>());
  auto const contents = nlohmann::json::parse(data);
  auto const status = message["attributes"].value("status", "");
  if (kSkippedStatus.count(status) != 0) {
    return LogInfo("skipped because status is " + status);
  }

  auto const trigger_type =
      contents["substitutions"].value("_TRIGGER_TYPE", "");
  if (trigger_type != "pr") {
    return LogInfo("skipping non-PR build");
  }

  auto const pr = contents["substitutions"].value("_PR_NUMBER", "");
  auto const build_name = contents["substitutions"].value("_BUILD_NAME", "");
  auto const image = contents["substitutions"].value("_IMAGE", "");
  auto const sha = contents["substitutions"].value("COMMIT_SHA", "");
  auto const prefix = "logs/google-cloud-cpp/" + pr + "/" + sha + "/";
  auto const project = contents["projectId"].get<std::string>();

  auto const html_head = HtmlHead(pr, sha);
  // Each element vector should contain exactly two elements.
  std::vector<std::vector<std::string>> preamble;
  preamble.emplace_back(std::vector<std::string>{
      "Repo", Anchor("https://github.com/googleapis/google-cloud-cpp")});
  preamble.emplace_back(std::vector<std::string>{
      "Pull Request", Anchor(kPrPrefix + pr, "#" + pr)});
  preamble.emplace_back(std::vector<std::string>{
      "Commit SHA", Anchor(kPrPrefix + pr + "/commits/" + sha, sha)});
  preamble.emplace_back(std::vector<std::string>{
      "GCB Console",
      Anchor(kGCBPrefix + project + "&query=tags%3D%22" + pr + "%22",
             "(requires auth)")});

  for (int i = 0; i != kAttempts; ++i) {
    auto const generation = [&] {
      auto meta =
          client.GetObjectMetadata(bucket_name(), prefix + destination());
      if (meta) return meta->generation();
      return std::int64_t{0};
    }();
    auto contents = CreateContents(client, prefix, html_head, preamble);
    // Use `IfGenerationMatch()` to prevent overwriting data. It is possible
    // that the data written concurrently was more up to date. Note that
    // (conveniently) `IfGenerationMatch(0)` means "if the object does not
    // exist".
    auto metadata = client.InsertObject(
        bucket_name(), prefix + destination(), contents,
        gcs::IfGenerationMatch(generation),
        gcs::WithObjectMetadata(gcs::ObjectMetadata{}
                                    .set_content_type("text/html")
                                    .set_cache_control("no-cache")));
    if (metadata.ok()) return;
    // If the write fail for any reason other than a failed precondition that is
    // an error.
    if (metadata.status().code() != StatusCode::kFailedPrecondition) {
      throw std::runtime_error(metadata.status().message());
    }
  }
}
