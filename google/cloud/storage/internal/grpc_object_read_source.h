// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_READ_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_READ_SOURCE_H

#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "absl/functional/function_ref.h"
#include <google/storage/v2/storage.pb.h>
#include <functional>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A data source for storage::ObjectReadStream using gRPC.
 *
 * This interfaces between the IOStream framework and the gRPC calls needed to
 * download the contents of a GCS object. The class holds the result of a
 * streaming RPC (a `grpc::ClientReader`) which downloads chunks of data as
 * needed. The IOStream classes (storage::ReadObjectStream,
 * storage::internal::ReadObjectStreambuf), read chunks from gRPC through this
 * class.
 */
class GrpcObjectReadSource : public ObjectReadSource {
 public:
  using StreamingRpc = ::google::cloud::internal::StreamingReadRpc<
      google::storage::v2::ReadObjectResponse>;

  // A function to create timers. These should return a future, satisfied with
  // `false` if the timer was canceled, and with `true` if the timer triggered.
  using TimerSource = std::function<future<bool>()>;

  explicit GrpcObjectReadSource(TimerSource timer_source,
                                std::unique_ptr<StreamingRpc> stream);

  ~GrpcObjectReadSource() override = default;

  bool IsOpen() const override { return static_cast<bool>(stream_); }

  /// Actively close a download, even if not all the data has been read.
  StatusOr<HttpResponse> Close() override;

  /// Read more data from the download, returning any HTTP headers and error
  /// codes.
  StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) override;

 private:
  using BufferManager =
      absl::FunctionRef<std::pair<absl::string_view, std::size_t>(
          absl::string_view)>;

  void HandleResponse(ReadSourceResult& result,
                      google::storage::v2::ReadObjectResponse response,
                      BufferManager buffer_manager);

  TimerSource timer_source_;
  std::unique_ptr<StreamingRpc> stream_;

  // In some cases the gRPC response may contain more data than the buffer
  // provided by the application. This buffer stores any excess results.
  std::string spill_;
  // The current portion of `spill_` that has not been consumed.
  absl::string_view spill_view_;

  // The status of the request.
  google::cloud::Status status_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_READ_SOURCE_H
