// Copyright 2018 Google LLC
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

#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/curl_wrappers.h"
#include <gmock/gmock.h>
#include <openssl/crypto.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
// An empty callback just to test things.
extern "C" void test_cb(int /*mode*/, int /*type*/, char const* /*file*/,
                        int /*line*/) {}

/// @test Verify that installing the libraries
TEST(CurlWrappers, LockingDisabledTest) {
  // The test cannot execute in this case.
  if (!SslLibraryNeedsLocking(CurlSslLibraryId())) GTEST_SKIP();
  // Install a trivial callback, this should disable the installation of the
  // normal callbacks in the the curl wrappers.
  CRYPTO_set_locking_callback(test_cb);
  CurlInitializeOnce(Options{}.set<EnableCurlSslLockingOption>(true));
  EXPECT_FALSE(SslLockingCallbacksInstalled());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
