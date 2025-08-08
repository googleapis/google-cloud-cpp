// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/connection_logging.h"
#include "google/cloud/storage/internal/async/object_descriptor_connection_logging.h"
#include "google/cloud/storage/internal/async/reader_connection_logging.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/log.h"
#include "google/cloud/tracing_options.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage_experimental::AsyncConnection;
using ::google::cloud::storage_experimental::AsyncReaderConnection;
using ::google::cloud::storage_experimental::AsyncRewriterConnection;
using ::google::cloud::storage_experimental::AsyncWriterConnection;
using ::google::cloud::storage_experimental::ObjectDescriptorConnection;

class AsyncConnectionLogging : public AsyncConnection {
 public:
  explicit AsyncConnectionLogging(std::shared_ptr<AsyncConnection> child)
      : child_(std::move(child)) {}

  Options options() const override { return child_->options(); }

  future<StatusOr<google::storage::v2::Object>> InsertObject(
      InsertObjectParams p) override {
    // TODO(shubhamkaushal): - implement logging for insert connection
    return child_->InsertObject(std::move(p));
  }

  future<StatusOr<std::shared_ptr<ObjectDescriptorConnection>>> Open(
      OpenParams p) override {
    // TODO(shubhamkaushal): - implement logging for open connection
    return child_->Open(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncReaderConnection>>> ReadObject(
      ReadObjectParams p) override {
    GCP_LOG(INFO) << "ReadObject("
                  << "bucket=" << p.request.bucket()
                  << ", object=" << p.request.object() << ")";
    auto options = p.options;
    auto fut = child_->ReadObject(std::move(p));
    return fut.then(
        [options](auto f) -> StatusOr<std::unique_ptr<AsyncReaderConnection>> {
          auto r = f.get();
          if (!r) {
            GCP_LOG(ERROR) << "ReadObject failed: " << r.status();
            return std::move(r).status();
          }
          GCP_LOG(INFO) << "ReadObject succeeded";
          return MakeLoggingReaderConnection(options, *std::move(r));
        });
  }

  future<StatusOr<storage_experimental::ReadPayload>> ReadObjectRange(
      ReadObjectParams p) override {
    GCP_LOG(INFO) << "ReadObjectRange("
                  << "bucket=" << p.request.bucket()
                  << ", object=" << p.request.object() << ")";
    auto fut = child_->ReadObjectRange(std::move(p));
    return fut.then([](auto f) {
      auto result = f.get();
      if (!result.ok()) {
        GCP_LOG(ERROR) << "ReadObjectRange failed: " << result.status();
      } else {
        GCP_LOG(INFO) << "ReadObjectRange succeeded";
      }
      return result;
    });
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>>
  StartAppendableObjectUpload(AppendableUploadParams p) override {
    // TODO(shubhamkaushal): - implement logging for start appendable upload
    // connection
    return child_->StartAppendableObjectUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>>
  ResumeAppendableObjectUpload(AppendableUploadParams p) override {
    // TODO(shubhamkaushal): - implement logging for resume appendable upload
    // connection
    return child_->ResumeAppendableObjectUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>>
  StartUnbufferedUpload(UploadParams p) override {
    // TODO(shubhamkaushal): - implement logging for start unbuffered upload
    // connection
    return child_->StartUnbufferedUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>> StartBufferedUpload(
      UploadParams p) override {
    // TODO(shubhamkaushal): - implement logging for start buffered upload
    // connection
    return child_->StartBufferedUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>>
  ResumeUnbufferedUpload(ResumeUploadParams p) override {
    // TODO(shubhamkaushal): - implement logging for resume unbuffered upload
    // connection
    return child_->ResumeUnbufferedUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>> ResumeBufferedUpload(
      ResumeUploadParams p) override {
    // TODO(shubhamkaushal): - implement logging for resume buffered upload
    // connection
    return child_->ResumeBufferedUpload(std::move(p));
  }

  future<StatusOr<google::storage::v2::Object>> ComposeObject(
      ComposeObjectParams p) override {
    // TODO(shubhamkaushal): - implement logging for compose connection
    return child_->ComposeObject(std::move(p));
  }

  future<Status> DeleteObject(DeleteObjectParams p) override {
    // TODO(shubhamkaushal): - implement logging for delete connection
    return child_->DeleteObject(std::move(p));
  }

  std::shared_ptr<AsyncRewriterConnection> RewriteObject(
      RewriteObjectParams p) override {
    // TODO(shubhamkaushal): - implement logging for rewrite connection
    return child_->RewriteObject(std::move(p));
  }

 private:
  std::shared_ptr<AsyncConnection> child_;
};

}  // namespace

std::shared_ptr<storage_experimental::AsyncConnection>
MakeLoggingAsyncConnection(
    std::shared_ptr<storage_experimental::AsyncConnection> impl) {
  auto const components = impl->options().get<LoggingComponentsOption>();
  if (std::find(components.begin(), components.end(), "rpc") ==
      components.end()) {
    return impl;
  }
  return std::make_shared<AsyncConnectionLogging>(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
