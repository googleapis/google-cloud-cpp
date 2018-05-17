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
#include "storage/client/internal/service_account_credentials.h"
#include <fstream>
#include <iostream>
#include <iterator>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::shared_ptr<Credentials> GoogleDefaultCredentials() {
  auto path = storage::internal::DefaultServiceAccountCredentialsFile();
  std::ifstream is(path);
  std::string jwt(std::istreambuf_iterator<char>{is}, {});
  return std::make_shared<storage::internal::ServiceAccountCredentials<>>(jwt);
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
