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

  std::string prefix = "test-prefix-coryan/";

  std::cout << "Received event"
            << "\n id: " << event.id()
            << "\n subject: " << event.subject().value_or("") << std::endl;
  std::cout << "Event: " << event.data().value_or("[no data]") << std::endl;

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
