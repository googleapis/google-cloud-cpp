// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/common_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

TEST(DefaultOptionsAsync, Basic) {
  auto const options = DefaultOptionsAsync({});
  auto const lwm = options.get<storage_experimental::BufferedUploadLwmOption>();
  auto const hwm = options.get<storage_experimental::BufferedUploadHwmOption>();
  EXPECT_GE(lwm, 256 * 1024);
  EXPECT_LT(lwm, hwm);
  // We use EndpointOption as a canary to test that most options are set.
  EXPECT_TRUE(options.has<EndpointOption>());
  EXPECT_THAT(options.get<EndpointOption>(), Not(IsEmpty()));
}

TEST(DefaultOptionsAsync, Adjust) {
  auto const options = DefaultOptionsAsync(
      Options{}
          .set<storage_experimental::BufferedUploadLwmOption>(16 * 1024)
          .set<storage_experimental::BufferedUploadHwmOption>(8 * 1024));
  auto const lwm = options.get<storage_experimental::BufferedUploadLwmOption>();
  auto const hwm = options.get<storage_experimental::BufferedUploadHwmOption>();
  EXPECT_GE(lwm, 256 * 1024);
  EXPECT_LT(lwm, hwm);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
