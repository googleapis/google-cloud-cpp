// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/polling_policy.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<PollingPolicy> DefaultPollingPolicy(
    internal::RPCPolicyParameters defaults) {
  return std::unique_ptr<PollingPolicy>(new GenericPollingPolicy<>(defaults));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<PollingPolicy> MakeCommonPollingPolicy(
    std::unique_ptr<bigtable::PollingPolicy> policy) {
  class CommonPollingPolicy : public PollingPolicy {
   public:
    explicit CommonPollingPolicy(std::unique_ptr<bigtable::PollingPolicy> impl)
        : impl_(std::move(impl)) {}
    ~CommonPollingPolicy() override = default;

    std::unique_ptr<PollingPolicy> clone() const override {
      return std::make_unique<CommonPollingPolicy>(impl_->clone());
    }
    bool OnFailure(google::cloud::Status const& status) override {
      return impl_->OnFailure(status);
    }
    std::chrono::milliseconds WaitPeriod() override {
      return impl_->WaitPeriod();
    }

   private:
    std::unique_ptr<bigtable::PollingPolicy> impl_;
  };

  return std::make_unique<CommonPollingPolicy>(std::move(policy));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
