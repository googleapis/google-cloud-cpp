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

#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Truly;

auto constexpr kTooManyFailuresCount = 2;
auto constexpr kIdempotencyTokenHeader = "x-goog-gcs-idempotency-token";
auto constexpr kTestAuthority = "test-only-authority.googleapis.com";

Options RetryTestOptions() {
  return Options{}
      .set<RetryPolicyOption>(
          LimitedErrorCountRetryPolicy(kTooManyFailuresCount).clone())
      .set<BackoffPolicyOption>(
          ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                   std::chrono::microseconds(1), 2.0)
              .clone())
      .set<IdempotencyPolicyOption>(AlwaysRetryIdempotencyPolicy().clone())
      .set<AuthorityOption>(kTestAuthority);
}

::testing::Matcher<Status> StoppedOnTooManyTransients(char const* api_name) {
  auto matches_api_name = [=](std::string const& name) {
    return ResultOf(
        "status metadata matches function name <" + name + ">",
        [](Status const& s) { return s.error_info().metadata(); },
        Contains(Pair("gcloud-cpp.retry.function", name)));
  };
  auto matches_exhausted = [=]() {
    return ResultOf(
        "status metadata matches exhausted retry policy reason",
        [](Status const& s) { return s.error_info().metadata(); },
        Contains(Pair("gcloud-cpp.retry.reason", "retry-policy-exhausted")));
  };
  return AllOf(
      StatusIs(TransientError().code(), HasSubstr(TransientError().message())),
      matches_api_name(api_name), matches_exhausted());
}

::testing::Matcher<Status> StoppedOnPermanentError(char const* api_name) {
  auto matches_api_name = [](std::string const& name) {
    return ResultOf(
        "status metadata matches function name <" + name + ">",
        [](Status const& s) { return s.error_info().metadata(); },
        Contains(Pair("gcloud-cpp.retry.function", name)));
  };
  auto matches_exhausted = []() {
    return ResultOf(
        "status metadata matches exhausted retry policy reason",
        [](Status const& s) { return s.error_info().metadata(); },
        Contains(Pair("gcloud-cpp.retry.reason", "permanent-error")));
  };
  return AllOf(
      StatusIs(PermanentError().code(), HasSubstr(PermanentError().message())),
      matches_api_name(api_name), matches_exhausted());
}

::testing::Matcher<std::vector<std::string>> RetryLoopUsesSingleToken() {
  return AllOf(Not(IsEmpty()),
               ResultOf(
                   "The retry loop uses a non-empty token",
                   [](std::vector<std::string> const& v) {
                     return v.empty() ? std::string{} : v.front();
                   },
                   Not(IsEmpty())),
               ResultOf(
                   "All tokens are equal",
                   [](std::vector<std::string> const& v) { return v; },
                   Truly([](std::vector<std::string> const& v) {
                     auto const s = v.empty() ? std::string{} : v.front();
                     return std::all_of(v.begin(), v.end(),
                                        [s](auto const& x) { return x == s; });
                   })));
}

::testing::Matcher<std::vector<std::string>> RetryLoopUsesOptions() {
  return AllOf(Not(IsEmpty()), Each(Eq(kTestAuthority)));
}

void CaptureIdempotencyToken(std::vector<std::string>& tokens,
                             rest_internal::RestContext const& context) {
  auto const& headers = context.headers();
  auto l = headers.find(kIdempotencyTokenHeader);
  if (l == headers.end()) return;
  tokens.insert(tokens.end(), l->second.begin(), l->second.end());
}

void CaptureAuthorityOption(std::vector<std::string>& authority,
                            rest_internal::RestContext const& context) {
  if (!context.options().has<AuthorityOption>()) return;
  authority.push_back(context.options().get<AuthorityOption>());
}

MockRetryClientFunction::MockRetryClientFunction(Status status)
    : status_(std::move(status)) {}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
