// Copyright 2020 Google LLC
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

#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/setenv.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

ScopedEnvironment::ScopedEnvironment(std::string variable,
                                     absl::optional<std::string> const& value)
    : variable_(std::move(variable)),
      prev_value_(internal::GetEnv(variable_.c_str())) {
  SetEnv(variable_.c_str(), value);
}

ScopedEnvironment::~ScopedEnvironment() {
  SetEnv(variable_.c_str(), std::move(prev_value_));
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
