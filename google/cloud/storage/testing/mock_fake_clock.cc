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

#include "google/cloud/storage/testing/mock_fake_clock.h"

namespace google {
namespace cloud {
namespace storage {
namespace testing {

// We need to define this value since it is static; set it to an arbitrary
// value.
std::time_t FakeClock::now_value_ = 1530060324;

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
