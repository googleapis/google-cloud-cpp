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

#include "google/cloud/storage/async/resume_policy.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class LimitedErrorCountResumePolicyImpl : public ResumePolicy {
 public:
  explicit LimitedErrorCountResumePolicyImpl(int maximum_resumes)
      : maximum_resumes_(maximum_resumes) {}
  ~LimitedErrorCountResumePolicyImpl() override = default;

  void OnStartSuccess() override {
    // For this policy we are only interested in the number of failures.
    // `OnStartSuccess()` is intended for policies that stop if the downloads
    // "fail too fast" or monitor some other condition that depends on when
    // the download started successfully and then failed.
  }
  Action OnFinish(Status const& s) override {
    if (!s.ok()) ++error_count_;
    return error_count_ > maximum_resumes_ ? kStop : kContinue;
  }

 private:
  int error_count_ = 0;
  int maximum_resumes_;
};

class UnlimitedErrorCountResumePolicyImpl : public ResumePolicy {
 public:
  UnlimitedErrorCountResumePolicyImpl() = default;
  ~UnlimitedErrorCountResumePolicyImpl() override = default;

  void OnStartSuccess() override {}
  Action OnFinish(Status const&) override { return kContinue; }
};

}  // namespace

ResumePolicy::~ResumePolicy() = default;

ResumePolicyFactory LimitedErrorCountResumePolicy(int maximum_resumes) {
  return [maximum_resumes]() -> std::unique_ptr<ResumePolicy> {
    return std::make_unique<LimitedErrorCountResumePolicyImpl>(maximum_resumes);
  };
}

ResumePolicyFactory UnlimitedErrorCountResumePolicy() {
  return []() -> std::unique_ptr<ResumePolicy> {
    return std::make_unique<UnlimitedErrorCountResumePolicyImpl>();
  };
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
