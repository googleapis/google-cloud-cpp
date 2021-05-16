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
#include <cppcodec/base64_rfc4648.hpp>
#include <curl/curl.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

struct BuildStatus {
  nlohmann::json build;
  std::string status;
};

// Parses the Pub/Sub message within the given `event`, and returns the build
// status and the embedded Build object from GCB. See also
// https://cloud.google.com/build/docs/api/reference/rest/v1/projects.builds#Build
BuildStatus ParseBuildStatus(google::cloud::functions::CloudEvent event) {
  if (event.data_content_type().value_or("") != "application/json") {
    throw std::runtime_error("expected application/json data");
  }
  auto const payload = nlohmann::json::parse(event.data().value_or("{}"));
  if (payload.count("message") == 0) {
    throw std::runtime_error("missing embedded Pub/Sub message");
  }
  auto const pubsub = payload["message"];
  if (pubsub.count("attributes") == 0 || pubsub.count("data") == 0) {
    throw std::runtime_error("missing Pub/Sub attributes or data");
  }
  auto const data = cppcodec::base64_rfc4648::decode<std::string>(
      pubsub["data"].get<std::string>());
  return BuildStatus{nlohmann::json::parse(data),
                     pubsub["attributes"].value("status", "")};
}

nlohmann::json MakeChatPayload(BuildStatus const& bs) {
  auto const trigger_name = bs.build["substitutions"].value("TRIGGER_NAME", "");
  auto const log_url = bs.build.value("logUrl", "");
  auto const status = bs.status;
  auto text = fmt::format("Build `{}`: *{}* {}", status, trigger_name, log_url);
  return nlohmann::json{{"text", std::move(text)}};
}

void HttpPost(std::string const& url, std::string const& data) {
  static constexpr auto kContentType = "Content-Type: application/json";
  using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
  auto const headers =
      Headers{curl_slist_append(nullptr, kContentType), curl_slist_free_all};
  using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
  auto curl = CurlHandle(curl_easy_init(), curl_easy_cleanup);
  if (!curl) throw std::runtime_error("Failed to create CurlHandle");
  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
  curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, data.c_str());
  CURLcode code = curl_easy_perform(curl.get());
  if (code != CURLE_OK) throw std::runtime_error(curl_easy_strerror(code));
}

}  // namespace

void SendBuildAlerts(google::cloud::functions::CloudEvent event) {
  static auto const webhook = [] {
    std::string const name = "GCB_BUILD_ALERT_WEBHOOK";
    auto const* env = std::getenv(name.c_str());
    if (env) return std::string{env};
    throw std::runtime_error("Missing environment variable: " + name);
  }();
  auto const bs = ParseBuildStatus(std::move(event));
  // https://cloud.google.com/build/docs/api/reference/rest/v1/projects.builds#Build.Status
  if (bs.status == "QUEUED" || bs.status == "WORKING" ||
      bs.status == "SUCCESS" || bs.status == "CANCELLED") {
    return;
  }
  auto const substitutions = bs.build["substitutions"];
  auto const trigger_type = substitutions.value("_TRIGGER_TYPE", "");
  auto const trigger_name = substitutions.value("TRIGGER_NAME", "");
  // Skips PR invocations and manually invoked builds (no trigger name).
  if (trigger_type == "pr" || trigger_name.empty()) return;
  auto const chat = MakeChatPayload(bs);
  std::cout << nlohmann::json{{"severity", "INFO"}, {"chat", chat}} << "\n";
  HttpPost(webhook, chat.dump());
}
