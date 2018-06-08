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

#include "google/cloud/storage/client_options.h"

namespace {
std::shared_ptr<storage::Credentials> StorageDefaultCredentials() {
  char const* emulator = std::getenv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
  if (emulator != nullptr) {
    return storage::CreateInsecureCredentials();
  }
  return storage::GoogleDefaultCredentials();
}
}  // namespace

namespace storage {
inline namespace STORAGE_CLIENT_NS {
ClientOptions::ClientOptions() : ClientOptions(StorageDefaultCredentials()) {}

ClientOptions::ClientOptions(std::shared_ptr<Credentials> credentials)
    : credentials_(std::move(credentials)),
      endpoint_("https://www.googleapis.com") {
  char const* emulator = std::getenv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
  if (emulator != nullptr) {
    endpoint_ = emulator;
  }
}
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
