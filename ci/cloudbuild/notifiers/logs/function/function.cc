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
#include <regex>
#include <sstream>
#include <stdexcept>

auto constexpr kGcsPrefix = "https://storage.googleapis.com/";
auto constexpr kAttempts = 4;

using ::google::cloud::StatusCode;
using ::google::cloud::functions::CloudEvent;
namespace gcs = ::google::cloud::storage;

void index_build_logs(CloudEvent event) {  // NOLINT
  static auto client = [] {
    return gcs::Client::CreateDefaultClient().value();
  }();
  static auto const bucket_name = [] {
    auto const* bname = std::getenv("BUCKET_NAME");
    if (bname == nullptr) {
      throw std::runtime_error("BUCKET_NAME environment variable is required");
    }
    return std::string{bname};
  }();

  if (event.data_content_type().value_or("") != "application/json") {
    std::cerr << nlohmann::json{{"severity", "error"},
                     {"message", "expected application/json data"},
                                }
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

  std::cout << "Contents: " << contents.dump() << "\n";
  std::cout << "Build Id: " << message["attributes"].value("buildId", "")
            << "\n";
  std::cout << "Attributes: " << message["attributes"].dump() << "\n";

  auto const trigger_type =
      contents["substitutions"].value("_TRIGGER_TYPE", "");
  if (trigger_type == "manual") {
    std::cout << nlohmann::json{{"severity", "info"},
                                {"message", "skip manual build"}}
                     .dump()
              << "\n";
    return;
  }

  auto const pr_number = contents["substitutions"].value("_PR_NUMBER", "");
  auto const commit_sha = contents["substitutions"].value("COMMIT_SHA", "");

  auto const prefix =
      "logs/google-cloud-cpp/" + pr_number + "/" + commit_sha + "/";

  auto const link_prefix = kGcsPrefix + bucket_name;

  static auto const kIsIndex =
      std::regex(R"re(/index\.html$)re", std::regex::optimize);
  static auto const kIsStep =
      std::regex(R"re(-step-[0-9]+\.txt)re", std::regex::optimize);

  std::int64_t index_generation = 0;
  for (int i = 0; i != kAttempts; ++i) {
    std::ostringstream os;
    os << "<!DOCTYPE html>\n";
    os << "<html><meta charset=\"utf-8\"><head>Logs for #" << pr_number << " @ "
       << commit_sha << "</head>\n";
    os << "<body>\n";
    os << "<ul>";
    for (auto const& object :
         client.ListObjects(bucket_name, gcs::Prefix(prefix))) {
      if (!object) throw std::runtime_error(object.status().message());
      if (std::regex_search(object->name(), kIsStep)) continue;
      if (std::regex_search(object->name(), kIsIndex)) {
        index_generation = object->generation();
        continue;
      }
      os << "<li><a href=\"" << kGcsPrefix + bucket_name + "/" + object->name()
         << "\">" << object->name() << "</a></li>\n";
    }
    os << "</ul>";
    os << "</body>\n";
    os << "</html>\n";
    // Use `IfGenerationMatch()` to prevent overwriting data. It is possible
    // that the data written concurrently was more up to date. Note that
    // (conveniently) `IfGenerationMatch(0)` means "if the object does not
    // exist".
    auto metadata = client.InsertObject(
        bucket_name, prefix + "index.html", os.str(),
        gcs::IfGenerationMatch(index_generation),
        gcs::WithObjectMetadata(gcs::ObjectMetadata{}
                                    .set_content_type("text/html")
                                    .set_cache_control("no-cache")));
    if (metadata.ok()) return;
    // Writing the index.html file
    if (metadata.status().code() != StatusCode::kFailedPrecondition) {
      throw std::runtime_error(metadata.status().message());
    }
  }
}
