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

nlohmann::json MakeChatPayload(nlohmann::json const& build) {
  auto const build_id = build.value("id", "unknown");
  auto const project_id = build.value("projectId", "unknown");
  auto const trigger_name = build["substitutions"].value("TRIGGER_NAME", "");
  auto text = fmt::format(
      R"""(Failed trigger: {trigger_name}
      (<https://console.cloud.google.com/cloud-build/builds/{build_id}?project={project_id}|Build Log>)
      )""",
      fmt::arg("trigger_name", trigger_name), fmt::arg("build_id", build_id),
      fmt::arg("project_id", project_id));
  return nlohmann::json{"text", std::move(text)};
} 

extern "C" size_t CurlOnWriteData(char* ptr, size_t size, size_t nmemb,
                                  void* userdata) {
  auto* buffer = reinterpret_cast<std::string*>(userdata);
  buffer->append(ptr, size * nmemb);
  return size * nmemb;
}

void HttpPost(std::string const& url, std::string const& data) {
  using CurlHeaders =
      std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
  auto headers = CurlHeaders(nullptr, curl_slist_free_all);
  auto add_header = [&headers](std::string const& h) {
    auto* nh = curl_slist_append(headers.get(), h.c_str());
    (void)headers.release();
    headers.reset(nh);
  };
  add_header("Content-Type: application/json; charset=UTF-8");

  using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
  auto easy = CurlHandle(curl_easy_init(), curl_easy_cleanup);

  auto setopt = [h = easy.get()](auto opt, auto value) {
    if (auto e = curl_easy_setopt(h, opt, value); e != CURLE_OK) {
      std::ostringstream os;
      os << "error [" << e << "] setting curl_easy option <" << opt
         << ">=" << value;
      throw std::runtime_error(std::move(os).str());
    }
  };
  auto get_response_code = [h = easy.get()]() {
    long code;  // NOLINT(google-runtime-int)
    auto e = curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
    if (e == CURLE_OK) {
      return static_cast<int>(code);
    }
    throw std::runtime_error("Cannot get response code");
  };

  setopt(CURLOPT_URL, url.c_str());
  setopt(CURLOPT_WRITEFUNCTION, &CurlOnWriteData);
  std::string buffer;
  setopt(CURLOPT_WRITEDATA, &buffer);
  setopt(CURLOPT_HTTPHEADER, headers.get());

  auto e = curl_easy_perform(easy.get());
  if (e != CURLE_OK) {
    LogError(fmt::format("Curl failed with code %d", get_response_code()));
  }
}

}  // namespace

void SendBuildAlerts(google::cloud::functions::CloudEvent event) {
  std::cerr << "Got event";
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
  auto const trigger_type =
      contents["substitutions"].value("_TRIGGER_TYPE", "");

  /* if (status != "FAILURE" || trigger_type  == "pr") { */
  /*   return LogDebug("Nothing to do"); */
  /* } */

  auto chat = MakeChatPayload(contents);
  std::cerr << "Got Chat payload: " << chat;

  std::string webhook_url;
  if (auto const* e = std::getenv("GCB_BUILD_ALERT_WEBHOOK")) {
    webhook_url = e;
  } else {
    return LogError("Missing GCB_BUILD_ALERT_WEBHOOK environment variable");
  }

  std::cerr << "Got webhook URL prefix" << webhook_url.substr(0, 10);
  HttpPost(webhook_url, chat.dump());
}
