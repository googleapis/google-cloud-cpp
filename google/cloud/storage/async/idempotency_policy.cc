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

#include "google/cloud/storage/async/idempotency_policy.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class StrictAsyncIdempotencyPolicy : public AsyncIdempotencyPolicy {
 public:
  ~StrictAsyncIdempotencyPolicy() override = default;
};

class AlwaysRetryAsyncIdempotencyPolicy : public AsyncIdempotencyPolicy {
 public:
  ~AlwaysRetryAsyncIdempotencyPolicy() override = default;

  google::cloud::Idempotency ReadObject(
      google::storage::v2::ReadObjectRequest const&) override {
    return Idempotency::kIdempotent;
  }

  google::cloud::Idempotency InsertObject(
      google::storage::v2::WriteObjectRequest const&) override {
    return Idempotency::kIdempotent;
  }

  google::cloud::Idempotency WriteObject(
      google::storage::v2::WriteObjectRequest const&) override {
    return Idempotency::kIdempotent;
  }

  google::cloud::Idempotency ComposeObject(
      google::storage::v2::ComposeObjectRequest const&) override {
    return Idempotency::kIdempotent;
  }

  google::cloud::Idempotency DeleteObject(
      google::storage::v2::DeleteObjectRequest const&) override {
    return Idempotency::kIdempotent;
  }

  google::cloud::Idempotency RewriteObject(
      google::storage::v2::RewriteObjectRequest const&) override {
    return Idempotency::kIdempotent;
  }
};

}  // namespace

AsyncIdempotencyPolicy::~AsyncIdempotencyPolicy() = default;

google::cloud::Idempotency AsyncIdempotencyPolicy::ReadObject(
    google::storage::v2::ReadObjectRequest const&) {
  // Read operations are always idempotent.
  return Idempotency::kIdempotent;
}

google::cloud::Idempotency AsyncIdempotencyPolicy::InsertObject(
    google::storage::v2::WriteObjectRequest const& request) {
  auto const& spec = request.write_object_spec();
  if (spec.has_if_generation_match() || spec.has_if_metageneration_match()) {
    return Idempotency::kIdempotent;
  }
  return Idempotency::kNonIdempotent;
}

google::cloud::Idempotency AsyncIdempotencyPolicy::WriteObject(
    google::storage::v2::WriteObjectRequest const&) {
  // Write requests for resumable uploads are (each part) always idempotent.
  // The initial StartResumableWrite() request has no visible side-effects. It
  // creates an upload id, but this cannot be queried if the response is lost.
  // The upload ids are also automatically garbage collected, and have no costs.
  //
  // Once the resumable upload id is created, the upload can succeed only once.
  return Idempotency::kIdempotent;
}

google::cloud::Idempotency AsyncIdempotencyPolicy::ComposeObject(
    google::storage::v2::ComposeObjectRequest const& request) {
  // Either of these pre-conditions will fail once the operation succeeds. Their
  // presence makes the operation idempotent.
  if (request.has_if_generation_match() ||
      request.has_if_metageneration_match()) {
    return Idempotency::kIdempotent;
  }
  return Idempotency::kNonIdempotent;
}

google::cloud::Idempotency AsyncIdempotencyPolicy::DeleteObject(
    google::storage::v2::DeleteObjectRequest const& request) {
  if (request.generation() != 0 || request.has_if_generation_match() ||
      request.has_if_metageneration_match()) {
    return Idempotency::kIdempotent;
  }
  return Idempotency::kNonIdempotent;
}

google::cloud::Idempotency AsyncIdempotencyPolicy::RewriteObject(
    google::storage::v2::RewriteObjectRequest const&) {
  // Rewrite requests are idempotent because they can only succeed once.
  return Idempotency::kIdempotent;
}

/// Creates an idempotency policy where only safe operations are retried.
std::unique_ptr<AsyncIdempotencyPolicy> MakeStrictAsyncIdempotencyPolicy() {
  return std::make_unique<StrictAsyncIdempotencyPolicy>();
}

/// Creates an idempotency policy that retries all operations.
std::unique_ptr<AsyncIdempotencyPolicy>
MakeAlwaysRetryAsyncIdempotencyPolicy() {
  return std::make_unique<AlwaysRetryAsyncIdempotencyPolicy>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
