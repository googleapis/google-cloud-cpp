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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_TESTING_ERROR_COLLECTORS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_TESTING_ERROR_COLLECTORS_H

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace cloud {
namespace generator_testing {

class ErrorCollector : public google::protobuf::DescriptorPool::ErrorCollector {
 public:
  ~ErrorCollector() override = default;

  void AddError(std::string const& filename, std::string const& element_name,
                google::protobuf::Message const*, ErrorLocation,
                std::string const& error_message) override;
};

class MultiFileErrorCollector
    : public google::protobuf::compiler::MultiFileErrorCollector {
 public:
  ~MultiFileErrorCollector() override = default;

  void AddError(std::string const& filename, int line, int column,
                std::string const& message) override;

  void AddWarning(std::string const& filename, int line, int column,
                  std::string const& message) override;
};

}  // namespace generator_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_TESTING_ERROR_COLLECTORS_H
