// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_RESUME_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_RESUME_POLICY_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Controls how the client library resumes uploads and downloads.
 */
class ResumePolicy {
 public:
  enum Action { kStop, kContinue };

  virtual ~ResumePolicy() = 0;

  /**
   * Notifies the policy about successful connections.
   *
   * Policies may need to know if a streaming RPC started successfully.  For
   * example, they may reset counters or timers when a streaming RPC starts.
   * The resume loop(s) will notify the policy of these events.
   */
  virtual void OnStartSuccess() = 0;

  /**
   * Controls what action the resume loop(s) should take when a streaming RPC
   * returns an error.
   */
  virtual Action OnFinish(Status const&) = 0;
};

/// Creates new instances of a `ResumePolicy`.
using ResumePolicyFactory = std::function<std::unique_ptr<ResumePolicy>()>;

/// Configure the resume policy used in a request, client, or connection.
struct ResumePolicyOption {
  using Type = ResumePolicyFactory;
};

/// Returns a factory which resumes up to @p maximum_resumes times.
ResumePolicyFactory LimitedErrorCountResumePolicy(int maximum_resumes);

/// Returns a factory which resumes as long as the previous attempt connected
/// successfully.
ResumePolicyFactory StopOnConsecutiveErrorsResumePolicy();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_RESUME_POLICY_H
