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

#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

#ifdef _WIN32
#include <crtdbg.h>
#endif  // _WIN32

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
void InitGoogleMock(int& argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  // On Windows, with MSVC, when compiling in Debug mode, the runtime creates a
  // popup dialog if an assertion fails. That is very convenient when doing
  // interactive debugging, but a real problem when running the unit tests in
  // batch mode. It is particularly bad when running the tests in a CI
  // environment because then the CI build hangs (until some time out), without
  // any errors in the log to debug the assertion failure.
#if defined(_WIN32) && defined(_MSC_VER)
  ::_set_error_mode(_OUT_TO_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif  // defined(_WIN32) && defined(_MSC_VER)
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
