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

#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/internal/raw_client_wrapper_utils.h"
#include <sstream>
#include <thread>

// Define the defaults using a pre-processor macro, this allows the application
// developers to change the defaults for their application by compiling with
// different values.
#ifndef STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD
#define STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD std::chrono::minutes(15)
#endif  // STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD

#ifndef STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY
#define STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY \
  std::chrono::milliseconds(10)
#endif  // STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY

#ifndef STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY
#define STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY std::chrono::minutes(5)
#endif  // STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY

#ifndef STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING
#define STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING 2.0
#endif  //  STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using namespace raw_client_wrapper_utils;

/**
 * Call a client operation with retries borrowing the RPC policies.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::Client object to make the call through.
 * @param retry_policy the policy controlling what failures are retryable, and
 *     for how long we can retry
 * @param backoff_policy the policy controlling how long to wait before
 *     retrying.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 * @throw std::exception with a description of the last error.
 */
template <typename MemberFunction>
static typename std::enable_if<
    CheckSignature<MemberFunction>::value,
    typename CheckSignature<MemberFunction>::ReturnType>::type
MakeCall(RetryPolicy& retry_policy, BackoffPolicy& backoff_policy,
         RawClient& client, MemberFunction function,
         typename CheckSignature<MemberFunction>::RequestType const& request,
         char const* error_message) {
  google::cloud::storage::Status last_status;
  while (not retry_policy.IsExhausted()) {
    auto result = (client.*function)(request);
    if (result.first.ok()) {
      return result;
    }
    last_status = std::move(result.first);
    if (not retry_policy.OnFailure(last_status)) {
      std::ostringstream os;
      if (retry_policy.IsExhausted()) {
        os << "Retry policy exhausted in " << error_message << ": "
           << last_status;
      } else {
        os << "Permanent error in " << error_message << ": " << last_status;
      }
      google::cloud::internal::RaiseRuntimeError(os.str());
    }
    auto delay = backoff_policy.OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << error_message << ": " << last_status;
  google::cloud::internal::RaiseRuntimeError(os.str());
}
}  // namespace

RetryClient::RetryClient(std::shared_ptr<RawClient> client,
                         DefaultPolicies unused)
    : client_(std::move(client)) {
  retry_policy_ =
      LimitedTimeRetryPolicy(STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD)
          .clone();
  backoff_policy_ =
      ExponentialBackoffPolicy(STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY,
                               STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY,
                               STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING)
          .clone();
}

ClientOptions const& RetryClient::client_options() const {
  return client_->client_options();
}

std::pair<Status, BucketMetadata> RetryClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  return MakeCall(*retry_policy, *backoff_policy, *client_,
                  &RawClient::GetBucketMetadata, request, __func__);
}

std::pair<Status, ObjectMetadata> RetryClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  return MakeCall(*retry_policy, *backoff_policy, *client_,
                  &RawClient::InsertObjectMedia, request, __func__);
}

std::pair<Status, ObjectMetadata> RetryClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  return MakeCall(*retry_policy, *backoff_policy, *client_,
                  &RawClient::GetObjectMetadata, request, __func__);
}

std::pair<Status, ReadObjectRangeResponse> RetryClient::ReadObjectRangeMedia(
    ReadObjectRangeRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  return MakeCall(*retry_policy, *backoff_policy, *client_,
                  &RawClient::ReadObjectRangeMedia, request, __func__);
}

std::pair<Status, ListObjectsResponse> RetryClient::ListObjects(
    ListObjectsRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  return MakeCall(*retry_policy, *backoff_policy, *client_,
                  &RawClient::ListObjects, request, __func__);
}

std::pair<Status, EmptyResponse> RetryClient::DeleteObject(
    DeleteObjectRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  return MakeCall(*retry_policy, *backoff_policy, *client_,
                  &RawClient::DeleteObject, request, __func__);
}

std::pair<Status, ListObjectAclResponse> RetryClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  return MakeCall(*retry_policy, *backoff_policy, *client_,
                  &RawClient::ListObjectAcl, request, __func__);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
