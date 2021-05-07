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
#include <cctype>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace {

void LogError(std::string const& msg) {
  auto const json = nlohmann::json{{"severity", "error"}, {"message", msg}};
  std::cerr << json.dump() << "\n";
}

auto MakeChatPayload(nlohmann::json const& build, std::string const& status) {
  auto const trigger_name = build["substitutions"].value("TRIGGER_NAME", "");
  auto const log_url = build.value("logUrl", "");
  auto text = fmt::format(
      R""(Failed build: *{trigger_name}* status={status} (<{log_url}|Build Log>))"",
      fmt::arg("status", status), fmt::arg("trigger_name", trigger_name),
      fmt::arg("log_url", log_url));
  return nlohmann::json{{"text", std::move(text)}};
}

void HttpPost(std::string const& url, std::string const& data) {
  static constexpr auto kContentType = "Content-Type: application/json";
  using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
  auto const headers = std::make_unique<Headers>(
      curl_slist_append(nullptr, kContentType), curl_slist_free_all);
  using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
  auto curl = CurlHandle(curl_easy_init(), curl_easy_cleanup);
  if (curl) {
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, data.c_str());
    CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) LogError(curl_easy_strerror(res));
  }
}

}  // namespace

void SendBuildAlerts(google::cloud::functions::CloudEvent event) {
  std::string webhook;
  if (auto const* e = std::getenv("GCB_BUILD_ALERT_WEBHOOK")) {
    webhook = e;
  } else {
    return LogError("Missing GCB_BUILD_ALERT_WEBHOOK environment variable");
  }
  if (event.data_content_type().value_or("") != "application/json") {
    return LogError("expected application/json data");
  }
  auto const payload = nlohmann::json::parse(event.data().value_or("{}"));
  if (payload.count("message") == 0) {
    return LogError("missing embedded Pub/Sub message");
  }
  auto const message = payload["message"];
  if (message.count("attributes") == 0 || message.count("data") == 0) {
    return LogError("missing Pub/Sub attributes or data");
  }
  auto const data = cppcodec::base64_rfc4648::decode<std::string>(
      message["data"].get<std::string>());
  auto const contents = nlohmann::json::parse(data);
  auto const status = message["attributes"].value("status", "");
  if (status == "SUCCESS") return;
  auto const chat = MakeChatPayload(contents, status);
  HttpPost(webhook, chat.dump());
}
