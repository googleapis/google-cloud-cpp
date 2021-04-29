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
#include <iostream>
#include <libgen.h>
#include <regex>
#include <sstream>
#include <stdexcept>
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
    os << "</tr>";
  }
  for (auto const& row : table) {
    os << "<tr>";
    for (auto const& col : row) {
      os << "<td>" << col << "</td>";
    }
    os << "</tr>\n";
  }
  os << "</table>";
}

}  // namespace

void IndexBuildLogs(CloudEvent event) {
  static auto client = [] {
    return gcs::Client::CreateDefaultClient().value();
  }();
  static auto const kBucketName = [] {
    auto const* bname = std::getenv("BUCKET_NAME");
    if (bname == nullptr) {
      throw std::runtime_error("BUCKET_NAME environment variable is required");
    }
    return std::string{bname};
  }();
  static auto const kDestination = [] {
    auto const* destination = std::getenv("DESTINATION");
    return std::string{destination == nullptr ? "index.html" : destination};
  }();

  // We skip any events with these status as such builds do not have a final
  // log, and cannot affect the output of the index.html file.
  static auto const kSkippedStatus = std::unordered_set<std::string>{
      "STATUS_UNKNOWN", "QUEUED", "WORKING",
      "",  // empty status indicates an invalid message
  };

  if (event.data_content_type().value_or("") != "application/json") {
    std::cerr << nlohmann::json{{"severity", "error"},
                                {"message", "expected application/json data"}}
                     .dump()
              << "\n";
    return;
  }
  auto const payload = nlohmann::json::parse(event.data().value_or("{}"));
  if (payload.count("message") == 0) {
    std::cerr << nlohmann::json{{"severity", "error"},
                                {"message", "missing embedded Pub/Sub message"}}
                     .dump()
              << "\n";
    return;
  }
  auto const message = payload["message"];
  if (message.count("attributes") == 0 or message.count("data") == 0) {
    std::cerr << nlohmann::json{{"severity", "error"},
                                {"message",
                                 "missing Pub/Sub attributes or data"}}
                     .dump()
              << "\n";
    return;
  }
  auto const data = cppcodec::base64_rfc4648::decode<std::string>(
      message["data"].get<std::string>());
  auto const contents = nlohmann::json::parse(data);
  auto const status = message["attributes"].value("status", "");
  if (kSkippedStatus.count(status) != 0) return;

  auto const trigger_type =
      contents["substitutions"].value("_TRIGGER_TYPE", "");
  if (trigger_type != "pr") {
    std::cout << nlohmann::json{{"severity", "info"},
                                {"message", "skipping non-PR build"}}
                     .dump()
              << "\n";
    return;
  }

  auto const pr = contents["substitutions"].value("_PR_NUMBER", "");
  auto const sha = contents["substitutions"].value("COMMIT_SHA", "");
  auto const prefix = "logs/google-cloud-cpp/" + pr + "/" + sha + "/";
  auto const project = contents["projectId"].get<std::string>();

  static auto const kIndexRE =
      std::regex("/" + kDestination + "$", std::regex::optimize);
  static auto const kLogfileRE =
      std::regex(R"re(/log-[0-9a-f-]+\.txt$)re", std::regex::optimize);

  // Each element vector should contain exactly two elements.
  std::vector<std::vector<std::string>> v;
  v.emplace_back(std::vector<std::string>{
      "Repo", Anchor("https://github.com/googleapis/google-cloud-cpp")});
  v.emplace_back(std::vector<std::string>{"Pull Request",
                                          Anchor(kPrPrefix + pr, "#" + pr)});
  v.emplace_back(std::vector<std::string>{
      "Commit SHA", Anchor(kPrPrefix + pr + "/commits/" + sha, sha)});
  v.emplace_back(std::vector<std::string>{
      "GCB Console",
      Anchor(kGCBPrefix + project + "&query=tags%3D%22" + pr + "%22",
             "(requires auth)")});

  std::int64_t index_generation = 0;
  for (int i = 0; i != kAttempts; ++i) {
    std::ostringstream os;
    os << "<!DOCTYPE html>\n";
    os << "<html>\n";
    os << "<head><meta charset=\"utf-8\">";
    os << "<title>"
       << "PR #" << pr << " google-cloud-cpp@" << sha.substr(0, 7)
       << "</title>\n";
    os << "<style>\n";
    os << "tr:nth-child(even) {background: #FFF}\n";
    os << "tr:nth-child(odd) {background: #DDD}\n";
    os << "</style></head>\n";
    os << "<body>\n";
    os << "<h1>Public Build Results</h1><hr/>";
    WriteTable(os, v);
    os << "<h2>Build logs</h2>";
    std::vector<std::vector<std::string>> table;
    std::vector<std::string> header{std::string{"Build"}, std::string{"Log"}};
    for (auto const& object :
         client.ListObjects(kBucketName, gcs::Prefix(prefix))) {
      if (!object) throw std::runtime_error(object.status().message());
      if (std::regex_search(object->name(), kIndexRE)) {
        index_generation = object->generation();
        continue;
      }
      if (!std::regex_search(object->name(), kLogfileRE)) continue;
      auto path = object->name();
      table.emplace_back(std::vector<std::string>{
          std::string{basename(dirname(path.data()))},
          Anchor(kGcsPrefix + kBucketName + "/" + object->name(), "raw log")});
    }
    WriteTable(os, table, header);
    os << "<hr/><p>NOTE: To debug a build on your local machine use the ";
    os << "<code>ci/cloudbuild/build.sh</code> script, with its ";
    os << "<a "
          "href=\"https://github.com/googleapis/google-cloud-cpp/blob/master/"
          "ci/cloudbuild/build.sh\">documentation here</a>.";
    os << "</p>\n";
    os << "</body>\n";
    os << "</html>\n";
    // Use `IfGenerationMatch()` to prevent overwriting data. It is possible
    // that the data written concurrently was more up to date. Note that
    // (conveniently) `IfGenerationMatch(0)` means "if the object does not
    // exist".
    auto metadata = client.InsertObject(
        kBucketName, prefix + kDestination, os.str(),
        gcs::IfGenerationMatch(index_generation),
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
