// Copyright 2025 Google LLC
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

#include "google/cloud/mtls_credentials_config.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(MtlsCredentialsConfigTest, Construction) {
  MtlsCredentialsConfig::Rest default_constructor;
  EXPECT_THAT(default_constructor.ssl_client_cert_filename(), IsEmpty());
  EXPECT_THAT(default_constructor.ssl_key_filename(), Eq(absl::nullopt));
  EXPECT_THAT(default_constructor.ssl_key_file_password(), Eq(absl::nullopt));
  EXPECT_THAT(default_constructor.ssl_cert_type(),
              Eq(MtlsCredentialsConfig::Rest::SslCertType::kPEM));
  EXPECT_THAT(MtlsCredentialsConfig::Rest::ToString(
                  default_constructor.ssl_cert_type()),
              Eq("PEM"));

  auto single_arg_constructor =
      MtlsCredentialsConfig::Rest{"my-cert-filename"}.set_cert_type(
          MtlsCredentialsConfig::Rest::SslCertType::kDER);
  EXPECT_THAT(single_arg_constructor.ssl_client_cert_filename(),
              Eq("my-cert-filename"));
  EXPECT_THAT(single_arg_constructor.ssl_key_filename(), Eq(absl::nullopt));
  EXPECT_THAT(single_arg_constructor.ssl_key_file_password(),
              Eq(absl::nullopt));
  EXPECT_THAT(single_arg_constructor.ssl_cert_type(),
              Eq(MtlsCredentialsConfig::Rest::SslCertType::kDER));
  EXPECT_THAT(MtlsCredentialsConfig::Rest::ToString(
                  single_arg_constructor.ssl_cert_type()),
              Eq("DER"));

  auto multi_arg_constructor =
      MtlsCredentialsConfig::Rest{"my-cert-filename", "my-ssl-key-filename",
                                  "my-ssl-key-file-password"}
          .set_cert_type(MtlsCredentialsConfig::Rest::SslCertType::kP12);
  EXPECT_THAT(multi_arg_constructor.ssl_client_cert_filename(),
              Eq("my-cert-filename"));
  EXPECT_THAT(multi_arg_constructor.ssl_key_filename(),
              Eq("my-ssl-key-filename"));
  EXPECT_THAT(multi_arg_constructor.ssl_key_file_password(),
              Eq("my-ssl-key-file-password"));
  EXPECT_THAT(multi_arg_constructor.ssl_cert_type(),
              Eq(MtlsCredentialsConfig::Rest::SslCertType::kP12));
  EXPECT_THAT(MtlsCredentialsConfig::Rest::ToString(
                  multi_arg_constructor.ssl_cert_type()),
              Eq("P12"));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
