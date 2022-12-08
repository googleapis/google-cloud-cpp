// Copyright 2022 Google LLC
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

#include "google/cloud/internal/oauth2_logging_credentials.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/log.h"
#include "absl/time/time.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

LoggingCredentials::LoggingCredentials(std::string phase,
                                       TracingOptions tracing_options,
                                       std::shared_ptr<Credentials> impl)
    : phase_(std::move(phase)),
      tracing_options_(std::move(tracing_options)),
      impl_(std::move(impl)) {}

LoggingCredentials::~LoggingCredentials() = default;

StatusOr<internal::AccessToken> LoggingCredentials::GetToken(
    std::chrono::system_clock::time_point now) {
  auto token = impl_->GetToken(now);
  if (!token) {
    GCP_LOG(DEBUG) << __func__ << "(" << phase_ << ") failed "
                   << token.status();
    return token;
  }
  if (now > token->expiration) {
    GCP_LOG(DEBUG) << __func__ << "(" << phase_ << "), token=" << *token
                   << ", token expired "
                   << absl::FormatDuration(
                          absl::FromChrono(now - token->expiration))
                   << " ago";
    return token;
  }
  GCP_LOG(DEBUG) << __func__ << "(" << phase_ << "), token=" << *token
                 << ", token will expire in "
                 << absl::FormatDuration(
                        absl::FromChrono(token->expiration - now));
  return token;
}

StatusOr<std::vector<std::uint8_t>> LoggingCredentials::SignBlob(
    absl::optional<std::string> const& signing_service_account,
    std::string const& string_to_sign) const {
  GCP_LOG(DEBUG) << __func__ << "(" << phase_ << "), signing_service_account="
                 << signing_service_account.value_or("<not set>")
                 << ", string_to_sign="
                 << internal::DebugString(string_to_sign, tracing_options_);
  return impl_->SignBlob(signing_service_account, string_to_sign);
}

std::string LoggingCredentials::AccountEmail() const {
  GCP_LOG(DEBUG) << __func__ << "(" << phase_ << ")";
  return impl_->AccountEmail();
}

std::string LoggingCredentials::KeyId() const {
  GCP_LOG(DEBUG) << __func__ << "(" << phase_ << ")";
  return impl_->KeyId();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
