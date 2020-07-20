// Copyright 2020 Google LLC
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

#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/setenv.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

ScopedEnvironment::ScopedEnvironment(std::string variable,
                                     absl::optional<std::string> const& value)
    : variable_(std::move(variable)),
      prev_value_(internal::GetEnv(variable_.c_str())) {
  internal::SetEnv(variable_.c_str(), value);
}

ScopedEnvironment::~ScopedEnvironment() {
  internal::SetEnv(variable_.c_str(), std::move(prev_value_));
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
