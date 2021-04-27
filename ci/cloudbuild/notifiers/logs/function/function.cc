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
#include <sstream>
#include <stdexcept>

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

  std::string prefix = "logs/google-cloud-cpp/" + pr_number + "/";

  // TODO(coryan) - this should be in a OCC loop
  std::ostringstream os;
  os << "<body>\n";
  os << "<ul>";
  for (auto const& object :
       client.ListObjects(bucket_name, gcs::Prefix(prefix))) {
    if (!object) throw std::runtime_error(object.status().message());
    os << "<li><a href=\"" << object->media_link() << "\">" << object->name()
       << "</a></li>\n";
  }
  os << "</ul>";
  os << "</body>\n";
  client
      .InsertObject(bucket_name, prefix + "index.html", os.str(),
                    gcs::ContentType("text/html"))
      .value();
}
