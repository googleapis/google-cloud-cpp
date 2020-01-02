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

#include "google/cloud/future.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status_or.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/tuple_filter.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/version.h"
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
  MinStreamSize(std::uintmax_t value) : value_(value) {}
  std::uintmax_t value() const { return value_; }

 private:
  std::uintmax_t value_;
};

namespace internal {

class ParallelObjectWriteStreambuf;

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
  SetOptionsApplyHelper(ResumableUploadRequest& request) : request(request) {}

  template <typename... Options>
  void operator()(Options&&... options) const {
    request.set_multiple_options(std::forward<Options>(options)...);
  }

 private:
  ResumableUploadRequest& request;
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
      std::string const& prefix, Options&&... options);

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
  // Type-erased function object to execute ComposeMany with most arguments
  // bound.
  using Composer = std::function<StatusOr<ObjectMetadata>(
      std::vector<ComposeSourceObject> const&)>;

  // The `ObjectWriteStream`s have to hold references to the state of
  // the parallel upload so that they can update it when finished and trigger
  // shards composition, hence `NonResumableParallelUploadState` has to be
  // destroyed after the `ObjectWriteStream`s.
  // `NonResumableParallelUploadState` and `ObjectWriteStream`s are passed
  // around by values, so we don't control their lifetime. In order to
  // circumvent it, we move the state to something held by a `shared_ptr`.
  class Impl : public std::enable_shared_from_this<Impl> {
   public:
    Impl(std::unique_ptr<ScopedDeleter> deleter, Composer composer);
    ~Impl();

    StatusOr<ObjectWriteStream> CreateStream(
        RawClient& raw_client, ResumableUploadRequest const& request);

    void StreamFinished(std::size_t stream_idx,
                        StatusOr<ResumableUploadResponse> const& response);

    future<StatusOr<ObjectMetadata>> WaitForCompletion() const;

    Status EagerCleanup();

    void Fail(Status status);

   private:
    mutable std::mutex mu_;
    // Promises made via `WaitForCompletion()`
    mutable std::vector<promise<StatusOr<ObjectMetadata>>> res_promises_;
    // Type-erased object for deleting temporary objects.
    std::unique_ptr<ScopedDeleter> deleter_;
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
      : impl_(std::move(state)), shards_(std::move(shards)) {}

  std::shared_ptr<Impl> impl_;
  std::vector<ObjectWriteStream> shards_;

  friend class ParallelObjectWriteStreambuf;
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
          typename std::enable_if<NotAmong<Options...>::template TPred<
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
  using internal::StaticTupleFilter;
  auto all_options = std::make_tuple(std::forward<Options>(options)...);
  auto delete_options =
      StaticTupleFilter<Among<QuotaUser, UserProject, UserIp>::TPred>(
          all_options);
  auto deleter = google::cloud::internal::make_unique<ScopedDeleter>(
      [client, bucket_name,
       delete_options](ObjectMetadata const& object) mutable {
        return google::cloud::internal::apply(
            DeleteApplyHelper{client, std::move(bucket_name), object.name()},
            std::tuple_cat(
                std::make_tuple(IfGenerationMatch(object.generation())),
                std::move(delete_options)));
      });

  auto compose_options = StaticTupleFilter<
      Among<DestinationPredefinedAcl, EncryptionKey, IfGenerationMatch,
            IfMetagenerationMatch, KmsKeyName, QuotaUser, UserIp, UserProject,
            WithObjectMetadata>::TPred>(all_options);
  auto composer = [client, bucket_name, object_name, compose_options, prefix](
                      std::vector<ComposeSourceObject> const& sources) mutable {
    return google::cloud::internal::apply(
        ComposeManyApplyHelper{client, std::move(bucket_name),
                               std::move(sources), prefix + ".compose_many",
                               std::move(object_name)},
        std::move(compose_options));
  };

  auto lock = internal::LockPrefix(client, bucket_name, prefix,
                                   std::make_tuple(options...));
  if (!lock) {
    return Status(
        lock.status().code(),
        "Failed to lock prefix for ParallelUpload: " + lock.status().message());
  }
  deleter->Add(*lock);

  auto internal_state = std::make_shared<NonResumableParallelUploadState::Impl>(
      std::move(deleter), std::move(composer));
  std::vector<ObjectWriteStream> streams;

  auto upload_options = StaticTupleFilter<
      Among<ContentEncoding, ContentType, DisableCrc32cChecksum, DisableMD5Hash,
            EncryptionKey, KmsKeyName, PredefinedAcl, UserProject,
            WithObjectMetadata>::TPred>(std::move(all_options));
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

/**
 * Return an empty option if Tuple contains an element of type T, otherwise
 *     return the value of the first element of type T
 */
template <typename T, typename Tuple, typename Enable = void>
struct ExtractFirstOccurenceOfTypeImpl {
  optional<T> operator()(Tuple const&) { return optional<T>(); }
};

template <typename T, typename... Options>
struct ExtractFirstOccurenceOfTypeImpl<
    T, std::tuple<Options...>,
    typename std::enable_if<
        Among<typename std::decay<Options>::type...>::template TPred<
            typename std::decay<T>::type>::value>::type> {
  optional<T> operator()(std::tuple<Options...> const& tuple) {
    return std::get<0>(StaticTupleFilter<Among<T>::template TPred>(tuple));
  }
};

template <typename T, typename Tuple>
optional<T> ExtractFirstOccurenceOfType(Tuple const& tuple) {
  return ExtractFirstOccurenceOfTypeImpl<T, Tuple>()(tuple);
}

struct PrepareParallelUploadApplyHelper {
  template <typename... Options>
  StatusOr<NonResumableParallelUploadState> operator()(Options&&... options) {
    return NonResumableParallelUploadState::Create(
        std::move(client), bucket_name, object_name, num_shards, prefix,
        std::forward<Options>(options)...);
  }

  Client client;
  std::string bucket_name;
  std::string object_name;
  std::size_t num_shards;
  std::string prefix;
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

 private:
  ParallelUploadFileShard(
      std::shared_ptr<NonResumableParallelUploadState> state,
      ObjectWriteStream ostream, std::string file_name,
      std::uintmax_t offset_in_file, std::uintmax_t bytes_to_upload,
      std::size_t upload_buffer_size)
      : state_(std::move(state)),
        ostream_(std::move(ostream)),
        file_name_(std::move(file_name)),
        offset_in_file_(offset_in_file),
        left_to_upload_(bytes_to_upload),
        upload_buffer_size_(upload_buffer_size) {}

  std::shared_ptr<NonResumableParallelUploadState> state_;
  ObjectWriteStream ostream_;
  std::string file_name_;
  std::uintmax_t offset_in_file_;
  std::uintmax_t left_to_upload_;
  std::size_t upload_buffer_size_;

  template <typename... Options>
  friend StatusOr<std::vector<ParallelUploadFileShard>> CreateUploadShards(
      Client client, std::string file_name, std::string bucket_name,
      std::string object_name, std::string prefix, Options&&... options);
};

/**
 * Prepare a parallel upload of a given file.
 *
 * The returned opaque objects reflect computed shards of the given file. Each
 * of them has an `Upload()` member function which will perform the upload of
 * that shard. You should parallelize running this function on them according to
 * your needs. You can affect how many shards will be created by using the
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
 * `UserProject`, `WithObjectMetadata`.
 *
 * @return the shards of the input file to be uploaded in parallel
 *
 * @par Idempotency
 * This operation is not idempotent. While each request performed by this
 * function is retried based on the client policies, the operation itself stops
 * on the first request that fails.
 *
 * @par Example
 * @snippet storage_object_samples.cc parallel upload file
 */
template <typename... Options>
StatusOr<std::vector<ParallelUploadFileShard>> CreateUploadShards(
    Client client, std::string file_name, std::string bucket_name,
    std::string object_name, std::string prefix, Options&&... options) {
  auto div_ceil = [](std::uintmax_t dividend, std::size_t divisor) {
    return (dividend + divisor - 1) / divisor;
  };
  // These defaults were obtained by experiments summarized in
  // https://github.com/googleapis/google-cloud-cpp/issues/2951#issuecomment-566237128
  MaxStreams const kDefaultMaxStreams(64);
  MinStreamSize const kDefaultMinStreamSize(32 * 1024 * 1024);

  std::error_code size_err;
  auto file_size = google::cloud::internal::file_size(file_name, size_err);
  if (size_err) {
    return Status(StatusCode::kNotFound, size_err.message());
  }

  auto const max_streams =
      ExtractFirstOccurenceOfType<MaxStreams>(std::tie(options...))
          .value_or(kDefaultMaxStreams)
          .value();

  std::uintmax_t const min_stream_size =
      (std::max<std::uintmax_t>)(1, ExtractFirstOccurenceOfType<MinStreamSize>(
                                        std::tie(options...))
                                        .value_or(kDefaultMinStreamSize)
                                        .value());

  std::size_t const wanted_num_streams =
      (std::max<std::size_t>)(1,
                              (std::min)(max_streams,
                                         div_ceil(file_size, min_stream_size)));

  std::uintmax_t const stream_size =
      (std::max<std::uintmax_t>)(1, div_ceil(file_size, wanted_num_streams));

  // This number might be less than wanted_num_streams.
  std::size_t const num_streams =
      (std::max<std::size_t>)(1U, div_ceil(file_size, stream_size));

  auto upload_buffer_size =
      client.raw_client()->client_options().upload_buffer_size();

  // Create the upload state.
  auto upload_options =
      StaticTupleFilter<NotAmong<MaxStreams, MinStreamSize>::TPred>(
          std::tie(options...));
  auto state = google::cloud::internal::apply(
      PrepareParallelUploadApplyHelper{
          std::move(client), std::move(bucket_name), std::move(object_name),
          num_streams, std::move(prefix)},
      std::move(upload_options));
  if (!state) {
    return state.status();
  }
  auto shared_state =
      std::make_shared<NonResumableParallelUploadState>(*std::move(state));
  std::vector<ParallelUploadFileShard> res;

  // Everything ready - we've got the shared state and the files open, let's
  // prepare the returned objects.
  for (std::size_t i = 0; i < num_streams; ++i) {
    std::uintmax_t offset = stream_size * i;
    std::uintmax_t size = (std::min)(file_size - stream_size * i, stream_size);
    res.emplace_back(ParallelUploadFileShard(
        shared_state, std::move(shared_state->shards()[i]), file_name, offset,
        size, upload_buffer_size));
  }
#if !defined(__clang__) && \
    (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ <= 9))
  // The extra std::move() is required to workaround a gcc-4.9 and gcc-4.8 bug,
  // which tries to copy the result otherwise.
  return std::move(res);
#else
  return res;
#endif
}

}  // namespace internal

template <typename... Options>
StatusOr<ObjectMetadata> ParallelUploadFile(
    Client client, std::string file_name, std::string bucket_name,
    std::string object_name, std::string prefix, bool ignore_cleanup_failures,
    Options&&... options) {
  auto shards = internal::CreateUploadShards(
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PARALLEL_UPLOAD_H_
