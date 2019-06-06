// Copyright 2018 Google LLC
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
#include <openssl/crypto.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
// An empty callback just to test things.
extern "C" void test_cb(int /*mode*/, int /*type*/, char const* /*file*/,
                        int /*line*/) {}

/// @test Verify that installing the libraries
TEST(CurlWrappers, LockingDisabledTest) {
  if (!SslLibraryNeedsLocking(CurlSslLibraryId())) {
    // The test cannot execute in this case.
    return;
  }
  // Install a trivial callback, this should disable the installation of the
  // normal callbacks in the the curl wrappers.
  CRYPTO_set_locking_callback(test_cb);
  CurlInitializeOnce(ClientOptions(oauth2::CreateAnonymousCredentials())
                         .set_enable_ssl_locking_callbacks(true));
  EXPECT_FALSE(SslLockingCallbacksInstalled());
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
