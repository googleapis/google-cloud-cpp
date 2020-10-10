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

#include "google/cloud/storage/internal/curl_wrappers.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include <gmock/gmock.h>
#include <csignal>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

extern "C" void test_handler(int) {}

/// @test Verify that configuring the library to disable the SIGPIPE handler
/// works as expected.
TEST(CurlWrappers, SigpipeHandlerDisabledTest) {
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
  // The memory sanitizer seems to intercept SIGPIPE, simply disable the test
  // in this case.
  GTEST_SKIP();
#endif  // __has_feature(memory_sanitizer)
#endif  // __has_feature

#if !defined(SIGPIPE)
  GTEST_SKIP();  // nothing to do
#elif CURL_AT_LEAST_VERSION(7, 30, 0)
  // libcurl <= 7.29.0 installs its own signal handler for SIGPIPE during
  // curl_global_init(). Unfortunately 7.29.0 is the default on CentOS-7, and
  // the tests here fails. We simply skip the test with this ancient library.
  auto initial_handler = std::signal(SIGPIPE, &test_handler);
  CurlInitializeOnce(ClientOptions(oauth2::CreateAnonymousCredentials())
                         .set_enable_sigpipe_handler(false));
  auto actual = std::signal(SIGPIPE, initial_handler);
  EXPECT_EQ(actual, &test_handler);
#endif  // defined(SIGPIPE)
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
