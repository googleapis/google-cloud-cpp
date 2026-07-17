// Copyright 2026 Google LLC
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

#include "google/cloud/internal/ssl_ec_curves.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

TEST(SslEcCurvesTest, PrependPqcEcCurveNotSupportedVersion) {
  std::vector<std::string> groups = {"P-256", "X25519MLKEM768"};
  auto actual = PrependPqcEcCurve(groups, /*supports_ssl_ec_curves=*/false);
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("libcurl version 7.73.0 or later")));
}

TEST(SslEcCurvesTest, PrependPqcEcCurveNotSupportedByLibrary) {
  std::vector<std::string> groups = {"P-256", "P-384"};
  auto actual = PrependPqcEcCurve(groups, /*supports_ssl_ec_curves=*/true);
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("X25519MLKEM768 is not supported")));
}

TEST(SslEcCurvesTest, PrependPqcEcCurveSuccess) {
  std::vector<std::string> groups = {"P-256", "X25519MLKEM768", "P-384"};
  auto actual = PrependPqcEcCurve(groups, /*supports_ssl_ec_curves=*/true);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "X25519MLKEM768:P-256:P-384");
}

TEST(SslEcCurvesTest, PrependPqcEcCurveCaseInsensitive) {
  std::vector<std::string> groups = {"P-256", "x25519mlkem768", "P-384"};
  auto actual = PrependPqcEcCurve(groups, /*supports_ssl_ec_curves=*/true);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "x25519mlkem768:P-256:P-384");
}

TEST(SslEcCurvesTest, GetPqcEcCurvesDoesNotCrash) {
  // We cannot assume the host machine has PQC support or OpenSSL 3+, but
  // calling GetPqcEcCurves() should execute cleanly without crashing.
  auto actual = GetPqcEcCurves();
  if (actual.ok()) {
    EXPECT_THAT(*actual, HasSubstr("X25519MLKEM768"));
  } else {
    EXPECT_THAT(actual.status().code(), StatusCode::kUnavailable);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
