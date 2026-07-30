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

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/storage/testing/upload_hash_cases.h"
#include <vector>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
namespace {
// These values were obtained using:
// /bin/echo -n '' > foo.txt && gcloud storage hash foo.txt
auto constexpr kEmptyStringCrc32cChecksum = "AAAAAA==";
auto constexpr kEmptyStringMD5Hash = "1B2M2Y8AsgTpgAmY7PhCfg==";

// /bin/echo -n 'The quick brown fox jumps over the lazy dog' > foo.txt
// gcloud storage hash foo.txt
auto constexpr kQuickFoxCrc32cChecksum = "ImIEBA==";
auto constexpr kQuickFoxMD5Hash = "nhB9nTcrtoJr2B01QqQZ1g==";

}  // namespace

std::vector<UploadHashCase> UploadHashCases() {
  return std::vector<UploadHashCase>{
      // clang-format off
      // DisableCrc32c == true, Crc32cChecksumValue == {} and change the MD5
      {"", "",                  ChecksumAlgorithm::kNone, Crc32cChecksumValue(),  MD5HashValue()},
      {"", kEmptyStringMD5Hash, ChecksumAlgorithm::kNone, Crc32cChecksumValue(),  MD5HashValue(kEmptyStringMD5Hash)},
      {"", kQuickFoxMD5Hash,    ChecksumAlgorithm::kMD5, Crc32cChecksumValue(),  MD5HashValue()},
      {"", kEmptyStringMD5Hash, ChecksumAlgorithm::kMD5, Crc32cChecksumValue(),  MD5HashValue(kEmptyStringMD5Hash)},

      // DisableCrc32c == true, Crc32cChecksumValue == kEmptyStringCrc32cChecksum and change the MD5
      {kEmptyStringCrc32cChecksum, "",                  ChecksumAlgorithm::kNone,  Crc32cChecksumValue(kEmptyStringCrc32cChecksum),   MD5HashValue()},
      {kEmptyStringCrc32cChecksum, kEmptyStringMD5Hash, ChecksumAlgorithm::kNone,  Crc32cChecksumValue(kEmptyStringCrc32cChecksum),   MD5HashValue(kEmptyStringMD5Hash)},
      {kEmptyStringCrc32cChecksum, kQuickFoxMD5Hash,    ChecksumAlgorithm::kMD5,  Crc32cChecksumValue(kEmptyStringCrc32cChecksum),  MD5HashValue()},
      {kEmptyStringCrc32cChecksum, kEmptyStringMD5Hash, ChecksumAlgorithm::kMD5,  Crc32cChecksumValue(kEmptyStringCrc32cChecksum),  MD5HashValue(kEmptyStringMD5Hash)},

      // DisableCrc32c == false, Crc32cChecksumValue == {} and change the MD5
      {kQuickFoxCrc32cChecksum, "",                  ChecksumAlgorithm::kCrc32c, Crc32cChecksumValue(),  MD5HashValue()},
      {kQuickFoxCrc32cChecksum, kEmptyStringMD5Hash, ChecksumAlgorithm::kCrc32c, Crc32cChecksumValue(),  MD5HashValue(kEmptyStringMD5Hash)},

      // clang-format on
  };
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#include "google/cloud/internal/diagnostics_pop.inc"
