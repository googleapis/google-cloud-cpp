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

#include "generator/testing/fake_source_tree.h"

namespace google {
namespace cloud {
namespace generator_testing {

FakeSourceTree::FakeSourceTree(std::map<std::string, std::string> files)
    : files_(std::move(files)) {}

void FakeSourceTree::Insert(std::string const& filename, std::string contents) {
  files_[filename] = std::move(contents);
}

google::protobuf::io::ZeroCopyInputStream* FakeSourceTree::Open(
    FilenameType filename) {
  auto iter = files_.find(std::string(filename));
  return iter == files_.end()
             ? nullptr
             : new google::protobuf::io::ArrayInputStream(
                   iter->second.data(), static_cast<int>(iter->second.size()));
}

}  // namespace generator_testing
}  // namespace cloud
}  // namespace google
