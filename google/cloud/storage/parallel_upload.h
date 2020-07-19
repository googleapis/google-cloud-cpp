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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PARALLEL_UPLOAD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PARALLEL_UPLOAD_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/tuple_filter.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/status_or.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <functional>
#include <mutex>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * A parameter type indicating the maximum number of streams to
 * `ParallelUploadFile`.
 */
class MaxStreams {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  MaxStreams(std::size_t value) : value_(value) {}
  std::size_t value() const { return value_; }

 private:
  std::size_t value_;
};

/**
 * A parameter type indicating the minimum stream size to `ParallelUploadFile`.
 *
 * If `ParallelUploadFile`, receives this option it will attempt to make sure
 * that every shard is at least this long. This might not apply to the last
 * shard because it will be the remainder of the division of the file.
 */
class MinStreamSize {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  MinStreamSize(std::uintmax_t value) : value_(value) {}
  std::uintmax_t value() const { return value_; }

 private:
  std::uintmax_t value_;
};

namespace internal {

class ParallelUploadFileShard;
struct CreateParallelUploadShards;

/**
 * Return an empty option if Tuple contains an element of type T, otherwise
 *     return the value of the first element of type T
 */
template <typename T, typename Tuple, typename Enable = void>
struct ExtractFirstOccurenceOfTypeImpl {
  absl::optional<T> operator()(Tuple const&) { return absl::optional<T>(); }
};

template <typename T, typename... Options>
struct ExtractFirstOccurenceOfTypeImpl<
    T, std::tuple<Options...>,
    typename std::enable_if<
        Among<typename std::decay<Options>::type...>::template TPred<
            typename std::decay<T>::type>::value>::type> {
  absl::optional<T> operator()(std::tuple<Options...> const& tuple) {
    return std::get<0>(StaticTupleFilter<Among<T>::template TPred>(tuple));
  }
};

template <typename T, typename Tuple>
absl::optional<T> ExtractFirstOccurenceOfType(Tuple const& tuple) {
  return ExtractFirstOccurenceOfTypeImpl<T, Tuple>()(tuple);
}

/**
 * An option for `PrepareParallelUpload` to associate opaque data with upload.
 *
 * This is used by `CreateUploadShards()` to store additional information in the
 * parallel upload persistent state. The additional information is where each
 * shard starts in the uploaded file.
 */
class ParallelUploadExtraPersistentState {
 public:
  std::string payload() && { return std::move(payload_); }
  std::string payload() const& { return payload_; }

 private:
  friend struct CreateParallelUploadShards;
  explicit ParallelUploadExtraPersistentState(std::string payload)
      : payload_(std::move(payload)) {}

  std::string payload_;
};

class ParallelObjectWriteStreambuf;

// Type-erased function object to execute ComposeMany with most arguments
// bound.
using Composer = std::function<StatusOr<ObjectMetadata>(
    std::vector<ComposeSourceObject> const&)>;

struct ParallelUploadPersistentState {
  struct Stream {
    std::string object_name;
    std::string resumable_session_id;
  };

  std::string ToString() const;
  static StatusOr<ParallelUploadPersistentState> FromString(
      std::string const& json_rep);

  std::string destination_object_name;
  std::int64_t expected_generation;
  std::string custom_data;
  std::vector<Stream> streams;
};

// The `ObjectWriteStream`s have to hold references to the state of
// the parallel upload so that they can update it when finished and trigger
// shards composition, hence `ResumableParallelUploadState` has to be
// destroyed after the `ObjectWriteStream`s.
// `ResumableParallelUploadState` and `ObjectWriteStream`s are passed
// around by values, so we don't control their lifetime. In order to
// circumvent it, we move the state to something held by a `shared_ptr`.
class ParallelUploadStateImpl
    : public std::enable_shared_from_this<ParallelUploadStateImpl> {
 public:
  ParallelUploadStateImpl(bool cleanup_on_failures,
                          std::string destination_object_name,
                          std::int64_t expected_generation,
                          std::shared_ptr<ScopedDeleter> deleter,
                          Composer composer);
  ~ParallelUploadStateImpl();

  StatusOr<ObjectWriteStream> CreateStream(
      RawClient& raw_client, ResumableUploadRequest const& request);

  void AllStreamsFinished(std::unique_lock<std::mutex>& lk);
  void StreamFinished(std::size_t stream_idx,
                      StatusOr<ResumableUploadResponse> const& response);

  void StreamDestroyed(std::size_t stream_idx);

  future<StatusOr<ObjectMetadata>> WaitForCompletion() const;

  Status EagerCleanup();

  void Fail(Status status);

  ParallelUploadPersistentState ToPersistentState() const;

  std::string custom_data() const {
    std::unique_lock<std::mutex> lk(mu_);
    return custom_data_;
  }

  void set_custom_data(std::string custom_data) {
    std::unique_lock<std::mutex> lk(mu_);
    custom_data_ = std::move(custom_data);
  }

  std::string resumable_session_id() {
    std::unique_lock<std::mutex> lk(mu_);
    return resumable_session_id_;
  }

  void set_resumable_session_id(std::string resumable_session_id) {
    std::unique_lock<std::mutex> lk(mu_);
    resumable_session_id_ = std::move(resumable_session_id);
  }

  void PreventFromFinishing() {
    std::unique_lock<std::mutex> lk(mu_);
    ++num_unfinished_streams_;
  }

  void AllowFinishing() {
    std::unique_lock<std::mutex> lk(mu_);
    if (--num_unfinished_streams_ == 0) {
      AllStreamsFinished(lk);
    }
  }

 private:
  struct StreamInfo {
    std::string object_name;
    std::string resumable_session_id;
    absl::optional<ComposeSourceObject> composition_arg;
    bool finished;
  };

  mutable std::mutex mu_;
  // Promises made via `WaitForCompletion()`
  mutable std::vector<promise<StatusOr<ObjectMetadata>>> res_promises_;
  // Type-erased object for deleting temporary objects.
  std::shared_ptr<ScopedDeleter> deleter_;
  // Type-erased function object to execute ComposeMany with most arguments
  // bound.
  std::function<StatusOr<ObjectMetadata>(std::vector<ComposeSourceObject>)>
      composer_;
  std::string destination_object_name_;
  std::int64_t expected_generation_;
  // Set when all streams are closed and composed but before cleanup.
  bool finished_;
  // Tracks how many streams are still written to.
  std::size_t num_unfinished_streams_;
  std::vector<StreamInfo> streams_;
  absl::optional<StatusOr<ObjectMetadata>> res_;
  Status cleanup_status_;
  std::string custom_data_;
  std::string resumable_session_id_;
};

struct ComposeManyApplyHelper {
  template <typename... Options>
  StatusOr<ObjectMetadata> operator()(Options&&... options) const {
    return ComposeMany(client, bucket_name, std::move(source_objects), prefix,
                       std::move(destination_object_name), true,
                       std::forward<Options>(options)...);
  }

  Client& client;
  std::string bucket_name;
  std::vector<ComposeSourceObject> source_objects;
  std::string prefix;
  std::string destination_object_name;
};

class SetOptionsApplyHelper {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  SetOptionsApplyHelper(ResumableUploadRequest& request) : request_(request) {}

  template <typename... Options>
  void operator()(Options&&... options) const {
    request_.set_multiple_options(std::forward<Options>(options)...);
  }

 private:
  ResumableUploadRequest& request_;
};

struct ReadObjectApplyHelper {
  template <typename... Options>
  ObjectReadStream operator()(Options&&... options) const {
    return client.ReadObject(bucket_name, object_name,
                             std::forward<Options>(options)...);
  }

  Client& client;
  std::string const& bucket_name;
  std::string const& object_name;
};

struct GetObjectMetadataApplyHelper {
  template <typename... Options>
  StatusOr<ObjectMetadata> operator()(Options... options) const {
    return client.GetObjectMetadata(bucket_name, object_name,
                                    std::move(options)...);
  }

  Client& client;
  std::string bucket_name;
  std::string object_name;
};

/**
 * A class representing an individual shard of the parallel upload.
 *
 * In order to perform a parallel upload of a file, you should call
 * `CreateUploadShards()` and it will return a vector of objects of this class.
 * You should execute the `Upload()` member function on them in parallel to
 * execute the upload.
 *
 * You can then obtain the status of the whole upload via `WaitForCompletion()`.
 */
class ParallelUploadFileShard {
 public:
  ParallelUploadFileShard(ParallelUploadFileShard const&) = delete;
  ParallelUploadFileShard& operator=(ParallelUploadFileShard const&) = delete;
  ParallelUploadFileShard(ParallelUploadFileShard&&) = default;
  ParallelUploadFileShard& operator=(ParallelUploadFileShard&&) = default;
  ~ParallelUploadFileShard();

  /**
   * Perform the upload of this shard.
   *
   * This function will block until the shard is completed, or a permanent
   *     failure is encountered, or the retry policy is exhausted.
   */
  Status Upload();

  /**
   * Asynchronously wait for completion of the whole upload operation (not only
   *     this shard).
   *
   * @return the returned future will become satisfied once the whole upload
   *     operation finishes (i.e. `Upload()` completes on all shards); on
   *     success, it will hold the destination object's metadata
   */
  future<StatusOr<ObjectMetadata>> WaitForCompletion() {
    return state_->WaitForCompletion();
  }

  /**
   * Cleanup all the temporary files
   *
   * The destruction of the last of these objects tied to a parallel upload will
   * cleanup of all the temporary files used in the process of that parallel
   * upload. If the cleanup fails, it will fail silently not to crash the
   * program.
   *
   * If you want to control the status of the cleanup, use this member function
   * to do it eagerly, before destruction.
   *
   * It is enough to call it on one of the objects, but it is not invalid to
   * call it on all objects.
   */
  Status EagerCleanup() { return state_->EagerCleanup(); }

  /**
   * Retrieve resumable session ID to allow for potential future resume.
   */
  std::string resumable_session_id() { return resumable_session_id_; }

 private:
  friend struct CreateParallelUploadShards;
  ParallelUploadFileShard(std::shared_ptr<ParallelUploadStateImpl> state,
                          ObjectWriteStream ostream, std::string file_name,
                          std::uintmax_t offset_in_file,
                          std::uintmax_t bytes_to_upload,
                          std::size_t upload_buffer_size)
      : state_(std::move(state)),
        ostream_(std::move(ostream)),
        file_name_(std::move(file_name)),
        offset_in_file_(offset_in_file),
        left_to_upload_(bytes_to_upload),
        upload_buffer_size_(upload_buffer_size),
        resumable_session_id_(state_->resumable_session_id()) {}

  std::shared_ptr<ParallelUploadStateImpl> state_;
  ObjectWriteStream ostream_;
  std::string file_name_;
  std::uintmax_t offset_in_file_;
  std::uintmax_t left_to_upload_;
  std::size_t upload_buffer_size_;
  std::string resumable_session_id_;
};

/**
 * The state controlling uploading a GCS object via multiple parallel streams.
 *
 * To use this class obtain the state via `PrepareParallelUpload` and then write
 * the data to the streams associated with each shard. Once writing is done,
 * close or destroy the streams.
 *
 * When all the streams are `Close`d or destroyed, this class will join the
 * them (via `ComposeMany`) into the destination object and set the value in
 * `future`s returned by `WaitForCompletion`.
 *
 * Parallel upload will create temporary files. Upon completion of the whole
 * operation, this class will attempt to remove them in its destructor, but if
 * they fail, they fail silently. In order to proactively cleanup these files,
 * one can call `EagerCleanup()`.
 */
class NonResumableParallelUploadState {
 public:
  template <typename... Options>
  static StatusOr<NonResumableParallelUploadState> Create(
      Client client, std::string const& bucket_name,
      std::string const& object_name, std::size_t num_shards,
      std::string const& prefix, std::tuple<Options...> options);

  /**
   * Asynchronously wait for completion of the whole upload operation.
   *
   * @return the returned future will have a value set to the destination object
   *     metadata when all the streams are `Close`d or destroyed.
   */
  future<StatusOr<ObjectMetadata>> WaitForCompletion() const {
    return impl_->WaitForCompletion();
  }

  /**
   * Cleanup all the temporary files
   *
   * The destruction of this object will perform cleanup of all the temporary
   * files used in the process of the parallel upload. If the cleanup fails, it
   * will fail silently not to crash the program.
   *
   * If you want to control the status of the cleanup, use this member function
   * to do it eagerly, before destruction.
   *
   * @return the status of the cleanup.
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
  std::vector<ObjectWriteStream>& shards() { return shards_; }

  /**
   * Fail the whole operation.
   *
   * If called before all streams are closed or destroyed, calling this
   * operation will prevent composing the streams into the final destination
   * object and return a failure via `WaitForCompletion()`.
   *
   * @param status the status to fail the operation with.
   */
  void Fail(Status status) { return impl_->Fail(std::move(status)); }

 private:
  NonResumableParallelUploadState(
      std::shared_ptr<ParallelUploadStateImpl> state,
      std::vector<ObjectWriteStream> shards)
      : impl_(std::move(state)), shards_(std::move(shards)) {}

  std::shared_ptr<ParallelUploadStateImpl> impl_;
  std::vector<ObjectWriteStream> shards_;

  friend class NonResumableParallelObjectWriteStreambuf;
  friend struct CreateParallelUploadShards;
};

/**
 * The state controlling uploading a GCS object via multiple parallel streams,
 * allowing for resuming.
 *
 * To use this class obtain the state via `PrepareParallelUpload` (with
 * `UseResumableUploadSession` option) and then write the data to the streams
 * associated with each shard. Once writing is done, close or destroy the
 * streams.
 *
 * When all the streams are `Close`d or destroyed, this class will join the
 * them (via `ComposeMany`) into the destination object and set the value in
 * `future`s returned by `WaitForCompletion`.
 *
 * Parallel upload will create temporary files. Upon successful completion of
 * the whole operation, this class will attempt to remove them in its
 * destructor, but if they fail, they fail silently. In order to proactively
 * cleanup these files, one can call `EagerCleanup()`.
 *
 * In oder to resume an interrupted upload, provide `UseResumableUploadSession`
 * to `PrepareParallelUpload` with value set to what `resumable_session_id()`
 * returns.
 */
class ResumableParallelUploadState {
 public:
  static std::string session_id_prefix() { return "ParUpl:"; }

  template <typename... Options>
  static StatusOr<ResumableParallelUploadState> CreateNew(
      Client client, std::string const& bucket_name,
      std::string const& object_name, std::size_t num_shards,
      std::string const& prefix, std::string const& extra_state,
      std::tuple<Options...> const& options);

  template <typename... Options>
  static StatusOr<ResumableParallelUploadState> Resume(
      Client client, std::string const& bucket_name,
      std::string const& object_name, std::size_t num_shards,
      std::string const& prefix, std::string const& resumable_session_id,
      std::tuple<Options...> options);

  /**
   * Retrieve the resumable session id.
   *
   * This value, if passed via `UseResumableUploadSession` option indicates that
   * an upload should be a continuation of the one which this object represents.
   */
  std::string resumable_session_id() { return resumable_session_id_; }

  /**
   * Asynchronously wait for completion of the whole upload operation.
   *
   * @return the returned future will have a value set to the destination object
   *     metadata when all the streams are `Close`d or destroyed.
   */
  future<StatusOr<ObjectMetadata>> WaitForCompletion() const {
    return impl_->WaitForCompletion();
  }

  /**
   * Cleanup all the temporary files
   *
   * The destruction of this object will perform cleanup of all the temporary
   * files used in the process of the parallel upload. If the cleanup fails, it
   * will fail silently not to crash the program.
   *
   * If you want to control the status of the cleanup, use this member function
   * to do it eagerly, before destruction.
   *
   * @return the status of the cleanup.
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
  std::vector<ObjectWriteStream>& shards() { return shards_; }

  /**
   * Fail the whole operation.
   *
   * If called before all streams are closed or destroyed, calling this
   * operation will prevent composing the streams into the final destination
   * object and return a failure via `WaitForCompletion()`.
   *
   * @param status the status to fail the operation with.
   */
  void Fail(Status status) { return impl_->Fail(std::move(status)); }

 private:
  template <typename... Options>
  static std::shared_ptr<ScopedDeleter> CreateDeleter(
      Client client, std::string const& bucket_name,
      std::tuple<Options...> const& options);

  template <typename... Options>
  static Composer CreateComposer(Client client, std::string const& bucket_name,
                                 std::string const& object_name,
                                 std::int64_t expected_generation,
                                 std::string const& prefix,
                                 std::tuple<Options...> const& options);

  ResumableParallelUploadState(std::string resumable_session_id,
                               std::shared_ptr<ParallelUploadStateImpl> state,
                               std::vector<ObjectWriteStream> shards)
      : resumable_session_id_(std::move(resumable_session_id)),
        impl_(std::move(state)),
        shards_(std::move(shards)) {}

  std::string resumable_session_id_;
  std::shared_ptr<ParallelUploadStateImpl> impl_;
  std::vector<ObjectWriteStream> shards_;

  friend class ResumableParallelObjectWriteStreambuf;
  friend struct CreateParallelUploadShards;
};

/**
 * Prepare a parallel upload state.
 *
 * The returned `NonResumableParallelUploadState` will contain streams to which
 * data can be uploaded in parallel.
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
 */
template <typename... Options,
          typename std::enable_if<
              NotAmong<typename std::decay<Options>::type...>::template TPred<
                  UseResumableUploadSession>::value,
              int>::type EnableIfNotResumable = 0>
StatusOr<NonResumableParallelUploadState> PrepareParallelUpload(
    Client client, std::string const& bucket_name,
    std::string const& object_name, std::size_t num_shards,
    std::string const& prefix, Options&&... options) {
  return NonResumableParallelUploadState::Create(
      std::move(client), bucket_name, object_name, num_shards, prefix,
      StaticTupleFilter<NotAmong<ParallelUploadExtraPersistentState>::TPred>(
          std::forward_as_tuple(std::forward<Options>(options)...)));
}

template <typename... Options,
          typename std::enable_if<
              Among<typename std::decay<Options>::type...>::template TPred<
                  UseResumableUploadSession>::value,
              int>::type EnableIfResumable = 0>
StatusOr<ResumableParallelUploadState> PrepareParallelUpload(
    Client client, std::string const& bucket_name,
    std::string const& object_name, std::size_t num_shards,
    std::string const& prefix, Options&&... options) {
  auto resumable_args =
      StaticTupleFilter<Among<UseResumableUploadSession>::TPred>(
          std::tie(options...));
  static_assert(std::tuple_size<decltype(resumable_args)>::value == 1,
                "The should be exacly one UseResumableUploadSession argument");
  std::string resumable_session_id = std::get<0>(resumable_args).value();
  auto extra_state_arg =
      ExtractFirstOccurenceOfType<ParallelUploadExtraPersistentState>(
          std::tie(options...));

  auto forwarded_args =
      StaticTupleFilter<NotAmong<UseResumableUploadSession,
                                 ParallelUploadExtraPersistentState>::TPred>(
          std::forward_as_tuple(std::forward<Options>(options)...));

  if (resumable_session_id.empty()) {
    return ResumableParallelUploadState::CreateNew(
        std::move(client), bucket_name, object_name, num_shards, prefix,
        extra_state_arg ? std::move(extra_state_arg).value().payload()
                        : std::string(),
        std::move(forwarded_args));
  }
  return ResumableParallelUploadState::Resume(
      std::move(client), bucket_name, object_name, num_shards, prefix,
      resumable_session_id, std::move(forwarded_args));
}

template <typename... Options>
StatusOr<NonResumableParallelUploadState>
NonResumableParallelUploadState::Create(Client client,
                                        std::string const& bucket_name,
                                        std::string const& object_name,
                                        std::size_t num_shards,
                                        std::string const& prefix,
                                        std::tuple<Options...> options) {
  using internal::StaticTupleFilter;
  auto delete_options =
      StaticTupleFilter<Among<QuotaUser, UserProject, UserIp>::TPred>(options);
  auto deleter = std::make_shared<ScopedDeleter>(
      [client, bucket_name, delete_options](std::string const& object_name,
                                            std::int64_t generation) mutable {
        return google::cloud::internal::apply(
            DeleteApplyHelper{client, std::move(bucket_name), object_name},
            std::tuple_cat(std::make_tuple(IfGenerationMatch(generation)),
                           std::move(delete_options)));
      });

  auto compose_options = StaticTupleFilter<
      Among<DestinationPredefinedAcl, EncryptionKey, IfGenerationMatch,
            IfMetagenerationMatch, KmsKeyName, QuotaUser, UserIp, UserProject,
            WithObjectMetadata>::TPred>(options);
  auto composer = [client, bucket_name, object_name, compose_options, prefix](
                      std::vector<ComposeSourceObject> const& sources) mutable {
    return google::cloud::internal::apply(
        ComposeManyApplyHelper{client, std::move(bucket_name),
                               std::move(sources), prefix + ".compose_many",
                               std::move(object_name)},
        std::move(compose_options));
  };

  auto lock = internal::LockPrefix(client, bucket_name, prefix, options);
  if (!lock) {
    return Status(
        lock.status().code(),
        "Failed to lock prefix for ParallelUpload: " + lock.status().message());
  }
  deleter->Add(*lock);

  auto internal_state = std::make_shared<ParallelUploadStateImpl>(
      true, object_name, 0, std::move(deleter), std::move(composer));
  std::vector<ObjectWriteStream> streams;

  auto upload_options = StaticTupleFilter<
      Among<ContentEncoding, ContentType, DisableCrc32cChecksum, DisableMD5Hash,
            EncryptionKey, KmsKeyName, PredefinedAcl, UserProject,
            WithObjectMetadata>::TPred>(std::move(options));
  auto& raw_client = *client.raw_client_;
  for (std::size_t i = 0; i < num_shards; ++i) {
    ResumableUploadRequest request(
        bucket_name, prefix + ".upload_shard_" + std::to_string(i));
    google::cloud::internal::apply(SetOptionsApplyHelper(request),
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

template <typename... Options>
std::shared_ptr<ScopedDeleter> ResumableParallelUploadState::CreateDeleter(
    Client client,  // NOLINT(performance-unnecessary-value-param)
    std::string const& bucket_name, std::tuple<Options...> const& options) {
  using internal::StaticTupleFilter;
  auto delete_options =
      StaticTupleFilter<Among<QuotaUser, UserProject, UserIp>::TPred>(options);
  return std::make_shared<ScopedDeleter>(
      [client, bucket_name, delete_options](std::string const& object_name,
                                            std::int64_t generation) mutable {
        return google::cloud::internal::apply(
            DeleteApplyHelper{client, std::move(bucket_name), object_name},
            std::tuple_cat(std::make_tuple(IfGenerationMatch(generation)),
                           std::move(delete_options)));
      });
}

template <typename... Options>
Composer ResumableParallelUploadState::CreateComposer(
    Client client,  // NOLINT(performance-unnecessary-value-param)
    std::string const& bucket_name, std::string const& object_name,
    std::int64_t expected_generation, std::string const& prefix,
    std::tuple<Options...> const& options) {
  auto compose_options = std::tuple_cat(
      StaticTupleFilter<
          Among<DestinationPredefinedAcl, EncryptionKey, KmsKeyName, QuotaUser,
                UserIp, UserProject, WithObjectMetadata>::TPred>(options),
      std::make_tuple(IfGenerationMatch(expected_generation)));
  auto get_metadata_options = StaticTupleFilter<
      Among<DestinationPredefinedAcl, EncryptionKey, KmsKeyName, QuotaUser,
            UserIp, UserProject, WithObjectMetadata>::TPred>(options);
  auto composer =
      [client, bucket_name, object_name, compose_options, get_metadata_options,
       prefix](std::vector<ComposeSourceObject> const& sources) mutable
      -> StatusOr<ObjectMetadata> {
    auto res = google::cloud::internal::apply(
        ComposeManyApplyHelper{client, bucket_name, std::move(sources),
                               prefix + ".compose_many", object_name},
        std::move(compose_options));
    if (res) {
      return res;
    }
    if (res.status().code() != StatusCode::kFailedPrecondition) {
      return res.status();
    }
    // This means that the object already exists and it is not the object, which
    // existed upon start of parallel upload. For simplicity, we assume that
    // it's a result of a previously interrupted ComposeMany invocation.
    return google::cloud::internal::apply(
        GetObjectMetadataApplyHelper{client, std::move(bucket_name),
                                     std::move(object_name)},
        std::move(get_metadata_options));
  };
  return Composer(std::move(composer));
}

template <typename... Options>
StatusOr<ResumableParallelUploadState> ResumableParallelUploadState::CreateNew(
    Client client, std::string const& bucket_name,
    std::string const& object_name, std::size_t num_shards,
    std::string const& prefix, std::string const& extra_state,
    std::tuple<Options...> const& options) {
  using internal::StaticTupleFilter;

  auto get_object_meta_options = StaticTupleFilter<
      Among<IfGenerationMatch, IfGenerationNotMatch, IfMetagenerationMatch,
            IfMetagenerationNotMatch, UserProject>::TPred>(options);
  auto object_meta = google::cloud::internal::apply(
      GetObjectMetadataApplyHelper{client, bucket_name, object_name},
      std::move(get_object_meta_options));
  if (!object_meta && object_meta.status().code() != StatusCode::kNotFound) {
    return object_meta.status();
  }
  std::int64_t expected_generation =
      object_meta ? object_meta->generation() : 0;

  auto deleter = CreateDeleter(client, bucket_name, options);
  auto composer = CreateComposer(client, bucket_name, object_name,
                                 expected_generation, prefix, options);
  auto internal_state = std::make_shared<ParallelUploadStateImpl>(
      false, object_name, expected_generation, deleter, std::move(composer));
  internal_state->set_custom_data(std::move(extra_state));

  std::vector<ObjectWriteStream> streams;

  auto upload_options = std::tuple_cat(
      StaticTupleFilter<
          Among<ContentEncoding, ContentType, DisableCrc32cChecksum,
                DisableMD5Hash, EncryptionKey, KmsKeyName, PredefinedAcl,
                UserProject, WithObjectMetadata>::TPred>(options),
      std::make_tuple(UseResumableUploadSession("")));
  auto& raw_client = *client.raw_client_;
  for (std::size_t i = 0; i < num_shards; ++i) {
    ResumableUploadRequest request(
        bucket_name, prefix + ".upload_shard_" + std::to_string(i));
    google::cloud::internal::apply(SetOptionsApplyHelper(request),
                                   upload_options);
    auto stream = internal_state->CreateStream(raw_client, request);
    if (!stream) {
      return stream.status();
    }
    streams.emplace_back(*std::move(stream));
  }

  auto state_object_name = prefix + ".upload_state";
  auto insert_options = std::tuple_cat(
      std::make_tuple(IfGenerationMatch(0)),
      StaticTupleFilter<
          Among<PredefinedAcl, EncryptionKey, KmsKeyName, QuotaUser, UserIp,
                UserProject, WithObjectMetadata>::TPred>(options));
  auto state_object = google::cloud::internal::apply(
      InsertObjectApplyHelper{client, bucket_name, state_object_name,
                              internal_state->ToPersistentState().ToString()},
      std::move(insert_options));
  if (!state_object) {
    internal_state->Fail(state_object.status());
    return std::move(state_object).status();
  }
  std::string resumable_session_id = session_id_prefix() + state_object_name +
                                     ":" +
                                     std::to_string(state_object->generation());
  internal_state->set_resumable_session_id(resumable_session_id);
  deleter->Add(std::move(*state_object));
  return ResumableParallelUploadState(std::move(resumable_session_id),
                                      std::move(internal_state),
                                      std::move(streams));
}

StatusOr<std::pair<std::string, std::int64_t>> ParseResumableSessionId(
    std::string const& session_id);

template <typename... Options>
StatusOr<ResumableParallelUploadState> ResumableParallelUploadState::Resume(
    Client client, std::string const& bucket_name,
    std::string const& object_name, std::size_t num_shards,
    std::string const& prefix, std::string const& resumable_session_id,
    std::tuple<Options...> options) {
  using internal::StaticTupleFilter;

  auto state_and_gen = ParseResumableSessionId(resumable_session_id);
  if (!state_and_gen) {
    return state_and_gen.status();
  }

  auto read_options = std::tuple_cat(
      StaticTupleFilter<Among<DisableCrc32cChecksum, DisableMD5Hash,
                              EncryptionKey, Generation, UserProject>::TPred>(
          options),
      std::make_tuple(IfGenerationMatch(state_and_gen->second)));

  auto state_stream = google::cloud::internal::apply(
      ReadObjectApplyHelper{client, bucket_name, state_and_gen->first},
      std::move(read_options));
  std::string state_string(std::istreambuf_iterator<char>{state_stream}, {});
  state_stream.Close();

  auto persistent_state =
      ParallelUploadPersistentState::FromString(state_string);
  if (!persistent_state) {
    return persistent_state.status();
  }

  if (persistent_state->destination_object_name != object_name) {
    return Status(StatusCode::kInternal,
                  "Specified resumable session ID is doesn't match the "
                  "destination object name (" +
                      object_name + " vs " +
                      persistent_state->destination_object_name + ")");
  }
  if (persistent_state->streams.size() != num_shards && num_shards != 0) {
    return Status(StatusCode::kInternal,
                  "Specified resumable session ID is doesn't match the "
                  "previously specified number of shards (" +
                      std::to_string(num_shards) + " vs " +
                      std::to_string(persistent_state->streams.size()) + ")");
  }

  auto deleter = CreateDeleter(client, bucket_name, options);
  deleter->Add(state_and_gen->first, state_and_gen->second);
  auto composer =
      CreateComposer(client, bucket_name, object_name,
                     persistent_state->expected_generation, prefix, options);
  auto internal_state = std::make_shared<ParallelUploadStateImpl>(
      false, object_name, persistent_state->expected_generation, deleter,
      std::move(composer));
  internal_state->set_custom_data(std::move(persistent_state->custom_data));
  internal_state->set_resumable_session_id(resumable_session_id);
  // If a resumed stream is already finalized, callbacks from streams will be
  // executed immediately. We don't want them to trigger composition before all
  // of them are created.
  internal_state->PreventFromFinishing();
  std::vector<ObjectWriteStream> streams;

  auto upload_options = StaticTupleFilter<
      Among<ContentEncoding, ContentType, DisableCrc32cChecksum, DisableMD5Hash,
            EncryptionKey, KmsKeyName, PredefinedAcl, UserProject,
            WithObjectMetadata>::TPred>(std::move(options));
  auto& raw_client = *client.raw_client_;
  for (auto& stream_desc : persistent_state->streams) {
    ResumableUploadRequest request(bucket_name,
                                   std::move(stream_desc.object_name));
    google::cloud::internal::apply(
        SetOptionsApplyHelper(request),
        std::tuple_cat(upload_options,
                       std::make_tuple(UseResumableUploadSession(
                           std::move(stream_desc.resumable_session_id)))));
    auto stream = internal_state->CreateStream(raw_client, request);
    if (!stream) {
      internal_state->AllowFinishing();
      return stream.status();
    }
    streams.emplace_back(*std::move(stream));
  }

  internal_state->AllowFinishing();
  return ResumableParallelUploadState(std::move(resumable_session_id),
                                      std::move(internal_state),
                                      std::move(streams));
}

template <typename... Options>
std::vector<std::uintmax_t> ComputeParallelFileUploadSplitPoints(
    std::uintmax_t file_size, std::tuple<Options...> const& options) {
  auto div_ceil = [](std::uintmax_t dividend, std::uintmax_t divisor) {
    return (dividend + divisor - 1) / divisor;
  };
  // These defaults were obtained by experiments summarized in
  // https://github.com/googleapis/google-cloud-cpp/issues/2951#issuecomment-566237128
  MaxStreams const default_max_streams(64);
  MinStreamSize const default_min_stream_size(32 * 1024 * 1024);

  auto const min_stream_size =
      (std::max<std::uintmax_t>)(1, ExtractFirstOccurenceOfType<MinStreamSize>(
                                        options)
                                        .value_or(default_min_stream_size)
                                        .value());
  auto const max_streams = ExtractFirstOccurenceOfType<MaxStreams>(options)
                               .value_or(default_max_streams)
                               .value();

  auto const wanted_num_streams =
      (std::max<
          std::uintmax_t>)(1, (std::min<std::uintmax_t>)(max_streams,
                                                         div_ceil(
                                                             file_size,
                                                             min_stream_size)));

  auto const stream_size =
      (std::max<std::uintmax_t>)(1, div_ceil(file_size, wanted_num_streams));

  std::vector<std::uintmax_t> res;
  for (auto split = stream_size; split < file_size; split += stream_size) {
    res.push_back(split);
  }
  return res;
}

std::string ParallelFileUploadSplitPointsToString(
    std::vector<std::uintmax_t> const& split_points);

StatusOr<std::vector<std::uintmax_t>> ParallelFileUploadSplitPointsFromString(
    std::string const& s);

/**
 * Helper functor to call `PrepareParallelUpload` via `apply`.
 *
 * This object holds only references to objects, hence it should not be stored.
 * Instead, it should be used only as a transient object allowing for calling
 * `PrepareParallelUpload` via `apply`.
 */
struct PrepareParallelUploadApplyHelper {
  // Some gcc versions crash on using decltype for return type here.
  template <typename... Options>
  StatusOr<typename std::conditional<
      Among<typename std::decay<Options>::type...>::template TPred<
          UseResumableUploadSession>::value,
      ResumableParallelUploadState, NonResumableParallelUploadState>::type>
  operator()(Options&&... options) {
    return PrepareParallelUpload(std::move(client), bucket_name, object_name,
                                 num_shards, prefix,
                                 std::forward<Options>(options)...);
  }

  Client client;
  std::string const& bucket_name;
  std::string const& object_name;
  std::size_t num_shards;
  std::string const& prefix;
};

struct CreateParallelUploadShards {
  /**
   * Prepare a parallel upload of a given file.
   *
   * The returned opaque objects reflect computed shards of the given file. Each
   * of them has an `Upload()` member function which will perform the upload of
   * that shard. You should parallelize running this function on them according
   * to your needs. You can affect how many shards will be created by using the
   * `MaxStreams` and `MinStreamSize` options.
   *
   * Any of the returned objects can be used for obtaining the metadata of the
   * resulting object.
   *
   * @param client the client on which to perform the operation.
   * @param file_name the path to the file to be uploaded
   * @param bucket_name the name of the bucket that will contain the object.
   * @param object_name the uploaded object name.
   * @param prefix the prefix with which temporary objects will be created.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DestinationPredefinedAcl`,
   *     `EncryptionKey`, `IfGenerationMatch`, `IfMetagenerationMatch`,
   *     `KmsKeyName`, `MaxStreams, `MinStreamSize`, `QuotaUser`, `UserIp`,
   *     `UserProject`, `WithObjectMetadata`, `UseResumableUploadSession`.
   *
   * @return the shards of the input file to be uploaded in parallel
   *
   * @par Idempotency
   * This operation is not idempotent. While each request performed by this
   * function is retried based on the client policies, the operation itself
   * stops on the first request that fails.
   *
   * @par Example
   * @snippet storage_object_file_transfer_samples.cc parallel upload file
   */
  template <typename... Options>
  static StatusOr<std::vector<ParallelUploadFileShard>> Create(
      Client client,  // NOLINT(performance-unnecessary-value-param)
      std::string file_name, std::string const& bucket_name,
      std::string const& object_name, std::string const& prefix,
      Options&&... options) {
    std::error_code size_err;
    auto file_size = google::cloud::internal::file_size(file_name, size_err);
    if (size_err) {
      return Status(StatusCode::kNotFound, size_err.message());
    }

    auto const resumable_session_id_arg =
        ExtractFirstOccurenceOfType<UseResumableUploadSession>(
            std::tie(options...));
    bool const new_session = !resumable_session_id_arg ||
                             resumable_session_id_arg.value().value().empty();
    auto upload_options =
        StaticTupleFilter<NotAmong<MaxStreams, MinStreamSize>::TPred>(
            std::tie(options...));

    std::vector<uintmax_t> file_split_points;
    std::size_t num_shards = 0;
    if (new_session) {
      file_split_points =
          ComputeParallelFileUploadSplitPoints(file_size, std::tie(options...));
      num_shards = file_split_points.size() + 1;
    }

    // Create the upload state.
    auto state = google::cloud::internal::apply(
        PrepareParallelUploadApplyHelper{client, bucket_name, object_name,
                                         num_shards, prefix},
        std::tuple_cat(
            std::move(upload_options),
            std::make_tuple(ParallelUploadExtraPersistentState(
                ParallelFileUploadSplitPointsToString(file_split_points)))));
    if (!state) {
      return state.status();
    }

    if (!new_session) {
      // We need to recreate the split points of the file.
      auto maybe_split_points =
          ParallelFileUploadSplitPointsFromString(state->impl_->custom_data());
      if (!maybe_split_points) {
        state->Fail(maybe_split_points.status());
        return std::move(maybe_split_points).status();
      }
      file_split_points = *std::move(maybe_split_points);
    }

    // Everything ready - we've got the shared state and the files open, let's
    // prepare the returned objects.
    auto upload_buffer_size =
        client.raw_client()->client_options().upload_buffer_size();

    file_split_points.emplace_back(file_size);
    assert(file_split_points.size() == state->shards().size());
    std::vector<ParallelUploadFileShard> res;
    std::uintmax_t offset = 0;
    std::size_t shard_idx = 0;
    for (auto shard_end : file_split_points) {
      res.emplace_back(ParallelUploadFileShard(
          state->impl_, std::move(state->shards()[shard_idx++]), file_name,
          offset, shard_end - offset, upload_buffer_size));
      offset = shard_end;
    }
#if !defined(__clang__) && \
    (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ <= 9))
    // The extra std::move() is required to workaround a gcc-4.9 and gcc-4.8
    // bug, which tries to copy the result otherwise.
    return std::move(res);
#elif defined(__clang__) && \
    (__clang_major__ < 4 || (__clang_major__ == 3 && __clang_minor__ <= 8))
    // The extra std::move() is required to workaround a Clang <= 3.8 bug, which
    // tries to copy the result otherwise.
    return std::move(res);
#else
    return res;
#endif
  }
};

/// @copydoc CreateParallelUploadShards::Create()
template <typename... Options>
StatusOr<std::vector<ParallelUploadFileShard>> CreateUploadShards(
    Client client,  // NOLxxxxxxINT(performance-unnecessary-value-param)
    std::string file_name, std::string const& bucket_name,
    std::string const& object_name, std::string const& prefix,
    Options&&... options) {
  return CreateParallelUploadShards::Create(
      std::move(client), std::move(file_name), bucket_name, object_name, prefix,
      std::forward<Options>(options)...);
}

}  // namespace internal

/**
 * Perform a parallel upload of a given file.
 *
 * You can affect how many shards will be created by using the `MaxStreams` and
 * `MinStreamSize` options.
 *
 * @param client the client on which to perform the operation.
 * @param file_name the path to the file to be uploaded
 * @param bucket_name the name of the bucket that will contain the object.
 * @param object_name the uploaded object name.
 * @param prefix the prefix with which temporary objects will be created.
 * @param ignore_cleanup_failures treat failures to cleanup the temporary
 *     objects as not fatal.
 * @param options a list of optional query parameters and/or request headers.
 *     Valid types for this operation include `DestinationPredefinedAcl`,
 *     `EncryptionKey`, `IfGenerationMatch`, `IfMetagenerationMatch`,
 *     `KmsKeyName`, `MaxStreams, `MinStreamSize`, `QuotaUser`, `UserIp`,
 *     `UserProject`, `WithObjectMetadata`, `UseResumableUploadSession`.
 *
 * @return the metadata of the object created by the upload.
 *
 * @par Idempotency
 * This operation is not idempotent. While each request performed by this
 * function is retried based on the client policies, the operation itself stops
 * on the first request that fails.
 *
 * @par Example
 * @snippet storage_object_file_transfer_samples.cc parallel upload file
 */
template <typename... Options>
StatusOr<ObjectMetadata> ParallelUploadFile(
    Client client, std::string file_name, std::string bucket_name,
    std::string object_name, std::string prefix, bool ignore_cleanup_failures,
    Options&&... options) {
  auto shards = internal::CreateParallelUploadShards::Create(
      std::move(client), std::move(file_name), std::move(bucket_name),
      std::move(object_name), std::move(prefix),
      std::forward<Options>(options)...);
  if (!shards) {
    return shards.status();
  }

  std::vector<std::thread> threads;
  threads.reserve(shards->size());
  for (auto& shard : *shards) {
    threads.emplace_back([&shard] {
      // We can safely ignore the status - if something fails we'll know
      // when obtaining final metadata.
      shard.Upload();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  auto res = (*shards)[0].WaitForCompletion().get();
  auto cleanup_res = (*shards)[0].EagerCleanup();
  if (!cleanup_res.ok() && !ignore_cleanup_failures) {
    return cleanup_res;
  }
  return res;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PARALLEL_UPLOAD_H
