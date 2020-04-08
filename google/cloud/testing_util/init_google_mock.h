// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_INIT_GOOGLE_MOCK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_INIT_GOOGLE_MOCK_H

#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/**
 * @deprecated
 * This function will be deleted soon. Callers should directly use
 * `::testing::InitGoogleMock` instead.
 *
 * We used to need to work around a gmock issue on Windows, but it has since
 * been fixed by https://github.com/google/googletest/pull/2372 and included in
 * googletest v1.10.0.
 *
 * See also https://github.com/googleapis/google-cloud-cpp/issues/3713.
 */
inline void InitGoogleMock(int& argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
}
}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_INIT_GOOGLE_MOCK_H
