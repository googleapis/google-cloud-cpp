// Copyright 2024 Google LLC
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
#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/async/retry_policy.h"
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include <limits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::size_t MinLwmValue() { return 256 * 1024; }

std::size_t MaxLwmValue() {
  if (std::numeric_limits<std::size_t>::digits < 64) {
    return std::numeric_limits<std::size_t>::max() / 4;
  }
  // Google Cloud storage objects are never larger than 5TiB. Not that any
  // application could reasonably use a 5TiB buffer.
  return static_cast<std::size_t>(5) * 1024 * 1024 * 1024L;
}

auto Lwm(Options const& opts) {
  auto v = opts.get<storage_experimental::BufferedUploadLwmOption>();
  if (v < MinLwmValue()) return MinLwmValue();
  if (v >= MaxLwmValue()) return MaxLwmValue();
  return v;
}

auto Hwm(Options const& opts) {
  auto lwm = Lwm(opts);
  auto v = opts.get<storage_experimental::BufferedUploadHwmOption>();
  if (v < 2 * lwm) return 2 * lwm;
  if (v >= 2 * MaxLwmValue()) return 2 * MaxLwmValue();
  return v;
}

auto Adjust(Options opts) {
  opts.set<storage_experimental::BufferedUploadLwmOption>(Lwm(opts));
  opts.set<storage_experimental::BufferedUploadHwmOption>(Hwm(opts));
  return opts;
}

auto constexpr kDefaultMaxRetryPeriod = std::chrono::minutes(15);

}  // namespace

Options DefaultOptionsAsync(Options opts) {
  opts = internal::MergeOptions(
      std::move(opts),
      Options{}
          .set<storage_experimental::AsyncRetryPolicyOption>(
              storage_experimental::LimitedTimeRetryPolicy(
                  kDefaultMaxRetryPeriod)
                  .clone())
          .set<storage_experimental::ResumePolicyOption>(
              storage_experimental::StopOnConsecutiveErrorsResumePolicy())
          .set<storage_experimental::IdempotencyPolicyOption>(
              storage_experimental::MakeStrictIdempotencyPolicy)
          .set<storage_experimental::EnableCrc32cValidationOption>(true)
          .set<storage_experimental::MaximumRangeSizeOption>(128 * 1024 *
                                                             1024L));
  return Adjust(DefaultOptionsGrpc(std::move(opts)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
