// Copyright 2018 Google LLC
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

#include "storage/client/credentials.h"
#include "storage/client/internal/authorized_user_credentials.h"
#include "storage/client/internal/google_application_default_credentials_file.h"
#include "storage/client/internal/nljson.h"
#include "google/cloud/internal/throw_delegate.h"
#include <fstream>
#include <iostream>
#include <iterator>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::shared_ptr<Credentials> GoogleDefaultCredentials() {
  auto path = storage::internal::GoogleApplicationDefaultCredentialsFile();
  std::ifstream is(path);
  std::string contents(std::istreambuf_iterator<char>{is}, {});

  auto object = storage::internal::nl::json::parse(contents);
  std::string type = object["type"];
  if (type == "authorized_user") {
    return std::make_shared<storage::internal::AuthorizedUserCredentials<>>(
        contents);
  }
  // TODO(#656) - support "service_account" credentials type.
  google::cloud::internal::RaiseRuntimeError("Unsupported credential type (" +
                                             type + ")");
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
