// Copyright 2019 Google LLC
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

#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/terminate_handler.h"
#include <cassert>
#include <cstdio>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

TempFile::TempFile(std::string const& content) {
  // This is obviously racy, but there is no portable way to create a
  // uniquely-named temporary file and know its name.
  char tmpfile_name[L_tmpnam];
  if (!std::tmpnam(tmpfile_name)) {
    Terminate("Failed to create a temporary file name");
  }
  std::ofstream f(tmpfile_name, std::ios::binary | std::ios::trunc);
  assert(f.good());
  f.write(content.data(), content.size());
  assert(f.good());

  name_ = tmpfile_name;
}

TempFile::~TempFile() {
  if (!name_.empty()) {
    std::remove(name_.c_str());
  }
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
