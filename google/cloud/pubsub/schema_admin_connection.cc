// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/schema_admin_connection.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/schema_options.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class WrapRetryPolicy : public google::cloud::pubsub::SchemaServiceRetryPolicy {
 public:
  explicit WrapRetryPolicy(std::unique_ptr<pubsub::RetryPolicy> child)
      : child_(std::move(child)) {}

  std::unique_ptr<google::cloud::pubsub::SchemaServiceRetryPolicy> clone()
      const override {
    return absl::make_unique<WrapRetryPolicy>(child_->clone());
  }
  bool OnFailure(Status const& status) override {
    return child_->OnFailure(status);
  }
  bool IsExhausted() const override { return child_->IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return child_->IsPermanentFailure(status);
  }

 private:
  void OnFailureImpl() override {}

  std::unique_ptr<pubsub::RetryPolicy> child_;
};

}  // namespace

std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(
    std::initializer_list<internal::NonConstructible>) {
  return MakeSchemaServiceConnection(Options{});
}

std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(Options opts) {
  if (opts.has<RetryPolicyOption>()) {
    opts.set<SchemaServiceRetryPolicyOption>(absl::make_unique<WrapRetryPolicy>(
        opts.get<RetryPolicyOption>()->clone()));
  }
  return MakeSchemaServiceConnection(std::move(opts));
}

std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto opts = internal::MakeOptions(options);
  if (retry_policy) opts.set<RetryPolicyOption>(retry_policy->clone());
  if (backoff_policy) opts.set<BackoffPolicyOption>(backoff_policy->clone());
  return MakeSchemaAdminConnection(std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
