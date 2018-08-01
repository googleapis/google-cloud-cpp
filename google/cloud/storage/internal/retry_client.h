// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_CLIENT_H_

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/retry_policy.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * A decorator for `storage::Client` that retries operations using policies.
 */
class RetryClient : public RawClient {
 public:
  struct DefaultPolicies {};
  explicit RetryClient(std::shared_ptr<RawClient> client,
                       DefaultPolicies unused);

  template <typename... Policies>
  explicit RetryClient(std::shared_ptr<RawClient> client,
                       Policies&&... policies)
      : RetryClient(std::move(client), DefaultPolicies{}) {
    ApplyPolicies(std::forward<Policies>(policies)...);
  }

  ~RetryClient() override = default;

  ClientOptions const& client_options() const override;

  std::pair<Status, ListBucketsResponse> ListBuckets(
      ListBucketsRequest const& request) override;

  std::pair<Status, BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) override;

  std::pair<Status, ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const& request) override;

  std::pair<Status, ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) override;

  std::pair<Status, std::unique_ptr<ObjectReadStreambuf>> ReadObject(
      ReadObjectRangeRequest const&) override;
  std::pair<Status, std::unique_ptr<ObjectWriteStreambuf>> WriteObject(
      InsertObjectStreamingRequest const&) override;

  std::pair<Status, ListObjectsResponse> ListObjects(
      ListObjectsRequest const&) override;

  std::pair<Status, EmptyResponse> DeleteObject(
      DeleteObjectRequest const&) override;

  std::pair<Status, ListObjectAclResponse> ListObjectAcl(
      ListObjectAclRequest const& request) override;
  std::pair<Status, ObjectAccessControl> CreateObjectAcl(
      CreateObjectAclRequest const&) override;
  std::pair<Status, EmptyResponse> DeleteObjectAcl(
      ObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> GetObjectAcl(
      ObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> UpdateObjectAcl(
      UpdateObjectAclRequest const&) override;

  std::shared_ptr<RawClient> client() const { return client_; }

 private:
  void Apply(RetryPolicy& policy) { retry_policy_ = policy.clone(); }

  void Apply(BackoffPolicy& policy) { backoff_policy_ = policy.clone(); }

  void ApplyPolicies() {}

  template <typename P, typename... Policies>
  void ApplyPolicies(P&& head, Policies&&... policies) {
    Apply(head);
    ApplyPolicies(std::forward<Policies>(policies)...);
  }

  std::shared_ptr<RawClient> client_;
  std::shared_ptr<RetryPolicy> retry_policy_;
  std::shared_ptr<BackoffPolicy> backoff_policy_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_CLIENT_H_
