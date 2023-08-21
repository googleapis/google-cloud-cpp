// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_UPLOAD_HASH_CASES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_UPLOAD_HASH_CASES_H

#include "google/cloud/storage/options.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

struct UploadHashCase {
  std::string crc32c_expected;
  std::string md5_expected;
  DisableCrc32cChecksum crc32_disabled;
  Crc32cChecksumValue crc32_value;
  DisableMD5Hash md5_disabled;
  MD5HashValue md5_value;
};

std::vector<UploadHashCase> UploadHashCases();

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_UPLOAD_HASH_CASES_H
