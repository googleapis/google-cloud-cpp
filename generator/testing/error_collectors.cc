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

#include "generator/testing/error_collectors.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_testing {

using ::testing::IsEmpty;

void ErrorCollector::RecordError(absl::string_view filename,
                                 absl::string_view element_name,
                                 google::protobuf::Message const*,
                                 ErrorLocation, absl::string_view message) {
  EXPECT_THAT(message, IsEmpty())
      << "filename=" << filename << ", element_name=" << element_name;
}

void MultiFileErrorCollector::RecordError(absl::string_view filename, int line,
                                          int column,
                                          absl::string_view message) {
  EXPECT_THAT(message, IsEmpty())
      << "filename=" << filename << ", line=" << line << ", column=" << column;
}

void MultiFileErrorCollector::RecordWarning(absl::string_view filename,
                                            int line, int column,
                                            absl::string_view message) {
  EXPECT_THAT(message, IsEmpty())
      << "filename=" << filename << ", line=" << line << ", column=" << column;
}

}  // namespace generator_testing
}  // namespace cloud
}  // namespace google
