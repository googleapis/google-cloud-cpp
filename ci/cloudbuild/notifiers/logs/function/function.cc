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

#include "generate_svg_badge.h"
#include <google/cloud/functions/cloud_event.h>
#include <google/cloud/storage/client.h>
#include <cppcodec/base64_rfc4648.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
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

auto Badge(std::string const& status) {
  auto l = status;
  std::transform(l.begin(), l.end(), l.begin(),
                 [](auto c) { return static_cast<char>(std::tolower(c)); });
  auto const color = [&]() -> std::string {
    if (status == "SUCCESS") return "brightgreen";
    if (status == "FAILURE") return "red";
    return "inactive";
  }();
  auto const kFormat =
      R"""(<image src="https://img.shields.io/badge/status-{}-{}?style=flat-square" alt="{}">)""";
  return fmt::format(kFormat, l, color, status);
}

auto ReproCommand(std::string const& distro, std::string const& build_name) {
  if (distro.empty() || build_name.empty()) return std::string{};
  auto const kFormat =
      R"""(<code>ci/cloudbuild/build.sh --docker --distro {} {}</code>)""";
  return fmt::format(kFormat, distro, build_name);
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
  os << "<head><meta charset=\"utf-8\">\n";
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
  std::vector<std::string> const header{"Build", "Log", "Status",
                                        "Repro Command"};
  std::vector<std::vector<std::string>> table;
  for (auto const& o : client.ListObjects(bucket_name(), gcs::Prefix(prefix))) {
    if (!o) throw std::runtime_error(o.status().message());
    if (!std::regex_search(o->name(), kLogfileRE)) continue;
    auto const path = std::filesystem::path(o->name());
    auto row = std::vector<std::string>{
        path.parent_path().filename().string(),
        Anchor(kGcsPrefix + bucket_name() + "/" + o->name(), "raw log")};
    auto const& m = o->metadata();
    auto value_or = [&m](std::string const& key) {
      auto const i = m.find(key);
      return i == m.end() ? std::string{} : i->second;
    };
    row.push_back(Badge(value_or("status")));
    row.push_back(ReproCommand(value_or("distro"), value_or("build_name")));
    table.emplace_back(std::move(row));
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

void LogDebug(std::string const& msg) {
  auto const kEnabled = [] {
    auto const* enabled = std::getenv("ENABLE_DEBUG");
    return enabled != nullptr;
  }();
  if (!kEnabled) return;
  std::cerr << LogFormat("debug", msg) << "\n";
}

auto LogsPrefix(std::string const& pr, std::string const& sha) {
  return "logs/google-cloud-cpp/" + pr + "/" + sha + "/";
}

void UpdateBuildBadge(gcs::Client client, nlohmann::json const& contents,
                      std::string const& status) {
  auto constexpr kBadgeLinkFormat =
      R"""(<meta http-equiv="refresh" content="0; url={link}" />)""";

  auto const build_id = contents.value("id", "");
  auto const build_name = contents["substitutions"].value("_BUILD_NAME", "");
  auto const distro = contents["substitutions"].value("_DISTRO", "");
  auto const sha = contents["substitutions"].value("COMMIT_SHA", "");

  auto const badge_image = GenerateSvgBadge(build_name, status);
  auto const badge_image_name =
      "badges/google-cloud-cpp/main/" + build_name + ".svg";

  auto link = kGcsPrefix + bucket_name() + "/" + LogsPrefix("main", sha) +
              distro + "-" + build_name + "/log-" + build_id + ".txt";
  auto badge_link = fmt::format(kBadgeLinkFormat, fmt::arg("link", link));
  auto const badge_link_name =
      "badges/google-cloud-cpp/main/" + build_name + ".html";

  client
      .InsertObject(
          bucket_name(), badge_image_name, badge_image,
          gcs::WithObjectMetadata(gcs::ObjectMetadata{}
                                      .set_content_type("image/svg+xml")
                                      .set_cache_control("no-cache")))
      .value();

  client
      .InsertObject(bucket_name(), badge_link_name, badge_link,
                    gcs::WithObjectMetadata(gcs::ObjectMetadata{}
                                                .set_content_type("text/html")
                                                .set_cache_control("no-cache")))
      .value();
}

void UpdateCurrentLogMetadata(gcs::Client client,
                              nlohmann::json const& contents,
                              std::string const& status) {
  auto const build_id = contents.value("id", "");
  auto const build_name = contents["substitutions"].value("_BUILD_NAME", "");
  auto const distro = contents["substitutions"].value("_DISTRO", "");

  auto const pr = contents["substitutions"].value("_PR_NUMBER", "");
  auto const sha = contents["substitutions"].value("COMMIT_SHA", "");
  auto const prefix = LogsPrefix(pr, sha);
  auto const object_name =
      prefix + distro + "-" + build_name + "/log-" + build_id + ".txt";
  LogDebug("object_name=" + object_name);
  LogDebug("distro=" + distro);
  LogDebug("build_name=" + build_name);
  LogDebug("contents=" + contents.dump());

  auto updated = client.PatchObject(bucket_name(), object_name,
                                    gcs::ObjectMetadataPatchBuilder{}
                                        .SetMetadata("distro", distro)
                                        .SetMetadata("build_name", build_name)
                                        .SetMetadata("status", status));
  std::ostringstream os;
  os << "updated metadata on " << object_name
     << ", result=" << updated.status().code();
  LogDebug(os.str());
}

void UpdateLogsIndex(gcs::Client client, nlohmann::json const& contents,
                     std::string const& status) {
  // We skip any events with these status as such builds do not have a final
  // log, and cannot affect the output of the index.html file.
  static auto const kSkippedStatus = std::unordered_set<std::string>{
      "STATUS_UNKNOWN", "QUEUED", "WORKING",
      "",  // empty status indicates an invalid message
  };

  if (kSkippedStatus.count(status) != 0) {
    return LogDebug("skip index generation because status is " + status);
  }

  auto const pr = contents["substitutions"].value("_PR_NUMBER", "");
  auto const sha = contents["substitutions"].value("COMMIT_SHA", "");
  auto const prefix = LogsPrefix(pr, sha);
  auto const html_head = HtmlHead(pr, sha);
  auto const project = contents["projectId"].get<std::string>();

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

}  // namespace

void IndexBuildLogs(CloudEvent event) {
  static auto client = [] {
    return gcs::Client::CreateDefaultClient().value();
  }();

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

  auto const trigger_type =
      contents["substitutions"].value("_TRIGGER_TYPE", "");

  if (trigger_type == "ci") return UpdateBuildBadge(client, contents, status);
  if (trigger_type != "pr") return LogDebug("skipping non-PR build");

  UpdateCurrentLogMetadata(client, contents, status);
  UpdateLogsIndex(client, contents, status);
}
