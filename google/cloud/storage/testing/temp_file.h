// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_TEMP_FILE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_TEMP_FILE_H

#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class TempFile {
 public:
  explicit TempFile(std::string const& content);
  TempFile(TempFile const&) = delete;
  TempFile& operator=(TempFile const&) = delete;
  TempFile& operator=(TempFile&&) = delete;

  ~TempFile();

  std::string name() const { return name_; }

 private:
  std::string name_;
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_TEMP_FILE_H
