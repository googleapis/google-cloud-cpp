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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_TESTING_FAKE_SOURCE_TREE_H
#define GOOGLE_CLOUD_CPP_GENERATOR_TESTING_FAKE_SOURCE_TREE_H

#include "google/cloud/internal/type_traits.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace generator_testing {

template <typename T, typename AlwaysVoid = void>
struct SourceTreeOpenArgType {
  using type = std::string const&;
};

template <typename T>
struct SourceTreeOpenArgType<
    T, google::cloud::internal::void_t<decltype(std::declval<T&>().Open(
           absl::string_view{}))>> {
  using type = absl::string_view;
};

class FakeSourceTree : public google::protobuf::compiler::SourceTree {
 public:
  FakeSourceTree() = default;
  explicit FakeSourceTree(std::map<std::string, std::string> files);

  void Insert(std::string const& filename, std::string contents);

  using FilenameType = typename generator_testing::SourceTreeOpenArgType<
      google::protobuf::compiler::SourceTree>::type;

  google::protobuf::io::ZeroCopyInputStream* Open(
      FilenameType filename) override;

 private:
  std::map<std::string, std::string> files_;
};

}  // namespace generator_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_TESTING_FAKE_SOURCE_TREE_H
