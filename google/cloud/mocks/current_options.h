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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MOCKS_CURRENT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MOCKS_CURRENT_OPTIONS_H

#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Retrieve options used in a client call.
 *
 * This would be used to verify configuration options from within a
 * MockConnection. It provides a way for applications to test the difference
 * between `client.Foo(request, options)` and `client.Foo(request)`.
 *
 * @code
 * TEST(Foo, CallOptions) {
 *   auto mock = std::make_shared<MockConnection>();
 *   EXPECT_CALL(*mock, Foo).WillOnce([] {
 *         auto const& options = google::cloud::mocks::CurrentOptions();
 *         EXPECT_THAT(options, ...);
 *       });
 *   auto client = Client(mock);
 *   MyFunctionThatCallsFoo(client);
 * }
 * @endcode
 */
inline Options const& CurrentOptions() { return internal::CurrentOptions(); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MOCKS_CURRENT_OPTIONS_H
