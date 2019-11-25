// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PARALLEL_UPLOAD_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PARALLEL_UPLOAD_H_

#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status_or.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/tuple_filter.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/version.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class ParallelObjectWriteStreambuf;

struct ComposeManyApplyHelper {
  template <typename... Options>
  StatusOr<ObjectMetadata> operator()(Options... options) const {
    return ComposeMany(client, bucket_name, std::move(source_objects), prefix,
                       std::move(destination_object_name), true,
                       std::move(options)...);
  }

  Client& client;
  std::string bucket_name;
  std::vector<ComposeSourceObject> source_objects;
  std::string prefix;
  std::string destination_object_name;
};

class SetOptionsApplyHelper {
 public:
  SetOptionsApplyHelper(internal::ResumableUploadRequest& request)
      : request(request) {}

  template <typename... Options>
  void operator()(Options... options) const {
    request.set_multiple_options(std::move(options)...);
  }

 private:
  internal::ResumableUploadRequest& request;
};

}  // namespace internal

/**
 * The state controlling uploading a GCS object via multiple parallel streams.
 *
 * The user is expected to obtain this state via `PrepareParallelUpload()` and
 * then write the data to streams available to the user in the `shards` member.
 * Once the writing is done, the user should close or destroy the streams.
 *
 * Data written to shards will be joined into the destination object after all
 * streams are `Close`d or destroyed.
 *
 * Once the data is uploaded, the `WaitForCompletion*` functions will return the
 * metadata of the destination object. They may be called multiple times.
 *
 * Temporary files are created in the process. They are attempted to be removed
 * in the destructor, but if they fail, they fail silently. In order to
 * proactively cleanup these files, one can call `EagerCleanup()`.
 */
class NonResumableParallelUploadState {
 public:
  template <typename... Options>
  static StatusOr<NonResumableParallelUploadState> Create(
      Client client, std::string const& bucket_name,
      std::string const& object_name, std::size_t num_shards,
      std::string const& prefix, Options&&... options);

  /**
   * Poll the operation for completion.
   *
   * The function blocks until all the streams have been `Close`d or destroyed.
   *
   * @return the destination object metadata
   */
  StatusOr<ObjectMetadata> WaitForCompletion() const {
    return impl_->WaitForCompletion();
  }

  /**
   * Poll the operation for completion.
   *
   * The function blocks until all the streams have been `Close`d or destroyed,
   * but no longer than `rel_time`.
   *
   * @param rel_time timeout duration
   * @return the destination object metadata or empty `optional` in case of
   *     timeout
   */
  template <class Rep, class Period>
  optional<StatusOr<ObjectMetadata>> WaitForCompletionFor(
      const std::chrono::duration<Rep, Period>& rel_time) const {
    return WaitForCompletionUntil(std::chrono::steady_clock::now() + rel_time);
  }

  /**
   * Poll the operation for completion.
   *
   * The function blocks until all the streams have been `Close`d or destroyed,
   * but no longer than until `timeout_time`.
   *
   * @param timeout_time time point when the function times out
   * @return the destination object metadata or empty `optional` in case of
   *     timeout
   */
  template <class Clock, class Duration>
  optional<StatusOr<ObjectMetadata>> WaitForCompletionUntil(
      const std::chrono::time_point<Clock, Duration>& timeout_time) const {
    return impl_->WaitForCompletionUntil(timeout_time);
  }

  /**
   * Cleanup all the temporary files
   *
   * The destruction of this object will perform cleanup of all the temporary
   * files used in the process of the paralell upload. If the cleanup fails, it
   * will fail silently not to crash the program.
   *
   * If you want to control the status of the cleanup, use this member function
   * to do it eagerly, before destruction.
   */
  Status EagerCleanup() { return impl_->EagerCleanup(); }

  /**
   * The streams to write to.
   *
   * When the streams are `Close`d, they will be concatenated into the
   * destination object in the same order as they appeared in this vector upon
   * this object's creation.
   *
   * It is safe to destroy or `std::move()` these streams.
   */
  std::vector<ObjectWriteStream> shards;

 private:
  using Composer =
      std::function<StatusOr<ObjectMetadata>(std::vector<ComposeSourceObject>)>;

  // The `ObjectWriteStream`s have to hold references to the state of
  // the parallel upload so that they can update it when finished and trigger
  // shards composition hence `NonResumableParallelUploadState` has to be
  // destroyed after the `ObjectWriteStream`s.
  // `NonResumableParallelUploadState` and `ObjectWriteStream`s are passed
  // around by values, so we don't control their lifetime. In order to
  // circumvent it, we move the state to something held by a `shared_ptr`.
  class Impl : public std::enable_shared_from_this<Impl> {
   public:
    // Type-erased function object to execute ComposeMany with most arguments
    // bound.
    Impl(std::unique_ptr<internal::ScopedDeleter> deleter, Composer composer);
    ~Impl();

    StatusOr<ObjectWriteStream> CreateStream(
        internal::RawClient& raw_client,
        internal::ResumableUploadRequest const& request);

    void StreamFinished(
        std::size_t stream_idx,
        StatusOr<internal::ResumableUploadResponse> const& response);

    // Poll for the operation to complete.
    template <class Clock, class Duration>
    optional<StatusOr<ObjectMetadata>> WaitForCompletionUntil(
        const std::chrono::time_point<Clock, Duration>& timeout_time) const {
      std::unique_lock<std::mutex> lk(mu_);
      if (!cv_.wait_until(lk, timeout_time, [this] { return finished_; })) {
        return optional<StatusOr<ObjectMetadata>>();
      }

      return *res_;
    }

    StatusOr<ObjectMetadata> WaitForCompletion() const;

    Status EagerCleanup();

   private:
    mutable std::mutex mu_;
    mutable std::condition_variable cv_;
    // Type-erased object for deleting temporary objects.
    std::unique_ptr<internal::ScopedDeleter> deleter_;
    // Type-erased function object to execute ComposeMany with most arguments
    // bound.
    std::function<StatusOr<ObjectMetadata>(std::vector<ComposeSourceObject>)>
        composer_;
    // Set when all streams are closed and composed but before cleanup.
    bool finished_;
    // Tracks how many streams are still written to.
    std::size_t num_unfinished_streams_;
    std::vector<ComposeSourceObject> to_compose_;
    google::cloud::optional<StatusOr<ObjectMetadata>> res_;
    Status cleanup_status_;
  };

  NonResumableParallelUploadState(std::shared_ptr<Impl> state,
                                  std::vector<ObjectWriteStream> shards)
      : shards(std::move(shards)), impl_(std::move(state)) {}

  std::shared_ptr<Impl> impl_;

  friend class internal::ParallelObjectWriteStreambuf;
};

/**
 * Prapre a parallel upload state.
 *
 * The returned `NonResumableParallelUploadState` will contained streams to
 * which data can be uploaded in parallel.
 *
 * @param client the client on which to perform the operation.
 * @param bucket_name the name of the bucket that will contain the object.
 * @param object_name the uploaded object name.
 * @param num_shards how many streams to upload the object through.
 * @param prefix the prefix with which temporary objects will be created.
 * @param options a list of optional query parameters and/or request headers.
 *     Valid types for this operation include `DestinationPredefinedAcl`,
 *     `EncryptionKey`, `IfGenerationMatch`, `IfMetagenerationMatch`,
 *     `KmsKeyName`, `QuotaUser`, `UserIp`, `UserProject`, `WithObjectMetadata`.
 *
 * @return the state of the parallel upload
 *
 * @par Idempotency
 * This operation is not idempotent. While each request performed by this
 * function is retried based on the client policies, the operation itself stops
 * on the first request that fails.
 *
 * @par Example
 * @snippet storage_object_samples.cc parallel upload
 */
template <
    typename... Options,
    typename std::enable_if<internal::NotAmong<Options...>::template TPred<
                                UseResumableUploadSession>::value,
                            int>::type enable_if_not_resumable = 0>
StatusOr<NonResumableParallelUploadState> PrepareParallelUpload(
    Client client, std::string const& bucket_name,
    std::string const& object_name, std::size_t num_shards,
    std::string const& prefix, Options&&... options) {
  return NonResumableParallelUploadState::Create(
      std::move(client), bucket_name, object_name, num_shards, prefix,
      std::forward<Options>(options)...);
}

template <typename... Options>
StatusOr<NonResumableParallelUploadState>
NonResumableParallelUploadState::Create(Client client,
                                        std::string const& bucket_name,
                                        std::string const& object_name,
                                        std::size_t num_shards,
                                        std::string const& prefix,
                                        Options&&... options) {
  using internal::Among;
  using internal::StaticTupleFilter;

  auto all_options = std::make_tuple(std::forward<Options>(options)...);
  auto delete_options =
      StaticTupleFilter<Among<QuotaUser, UserProject, UserIp>::TPred>(
          all_options);
  auto deleter = google::cloud::internal::make_unique<internal::ScopedDeleter>(
      [client, bucket_name,
       delete_options](ObjectMetadata const& object) mutable {
        return google::cloud::internal::apply(
            internal::DeleteApplyHelper{client, std::move(bucket_name),
                                        object.name()},
            std::tuple_cat(
                std::make_tuple(IfGenerationMatch(object.generation())),
                std::move(delete_options)));
      });

  auto compose_options = StaticTupleFilter<
      Among<DestinationPredefinedAcl, EncryptionKey, IfGenerationMatch,
            IfMetagenerationMatch, KmsKeyName, QuotaUser, UserIp, UserProject,
            WithObjectMetadata>::TPred>(all_options);
  auto composer = [client, bucket_name, object_name, compose_options,
                   prefix](std::vector<ComposeSourceObject> sources) mutable {
    return google::cloud::internal::apply(
        internal::ComposeManyApplyHelper{
            client, std::move(bucket_name), std::move(sources),
            prefix + ".compose_many", std::move(object_name)},
        std::move(compose_options));
  };

  auto internal_state = std::make_shared<NonResumableParallelUploadState::Impl>(
      std::move(deleter), std::move(composer));
  std::vector<ObjectWriteStream> streams;

  auto upload_options = StaticTupleFilter<
      Among<ContentEncoding, ContentType, DisableCrc32cChecksum, DisableMD5Hash,
            EncryptionKey, KmsKeyName, PredefinedAcl, UserProject,
            WithObjectMetadata>::TPred>(std::move(all_options));
  auto& raw_client = *client.raw_client_;
  for (std::size_t i = 0; i < num_shards; ++i) {
    internal::ResumableUploadRequest request(
        bucket_name, prefix + ".upload_shard_" + std::to_string(i));
    google::cloud::internal::apply(internal::SetOptionsApplyHelper(request),
                                   upload_options);
    auto stream = internal_state->CreateStream(raw_client, request);
    if (!stream) {
      return stream.status();
    }
    streams.emplace_back(*std::move(stream));
  }
  return NonResumableParallelUploadState(std::move(internal_state),
                                         std::move(streams));
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PARALLEL_UPLOAD_H_
