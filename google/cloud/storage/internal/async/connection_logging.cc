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
    GCP_LOG(INFO) << "InsertObject("
                  << "bucket="
                  << p.request.write_object_spec().resource().bucket()
                  << ", object="
                  << p.request.write_object_spec().resource().name() << ")";
    auto fut = child_->InsertObject(std::move(p));
    return fut.then([](auto f) {
      auto result = f.get();
      if (!result.ok()) {
        GCP_LOG(ERROR) << "InsertObject failed: " << result.status();
      } else {
        GCP_LOG(INFO) << "InsertObject succeeded";
      }
      return result;
    });
  }

  future<StatusOr<std::shared_ptr<ObjectDescriptorConnection>>> Open(
      OpenParams p) override {
    GCP_LOG(INFO) << "Open("
                  << "bucket=" << p.read_spec.bucket()
                  << ", object=" << p.read_spec.object() << ")";
    auto options = p.options;
    auto fut = child_->Open(std::move(p));
    return fut.then(
        [options](
            auto f) -> StatusOr<std::shared_ptr<ObjectDescriptorConnection>> {
          auto od = f.get();
          if (!od) {
            GCP_LOG(ERROR) << "Open failed: " << od.status();
            return std::move(od).status();
          }
          GCP_LOG(INFO) << "Open succeeded";
          return MakeLoggingObjectDescriptorConnection(*std::move(od), options);
        });
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
    // TODO(#15114) - implement logging for writer connections
    return child_->StartAppendableObjectUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>>
  ResumeAppendableObjectUpload(AppendableUploadParams p) override {
    // TODO(#15114) - implement logging for writer connections
    return child_->ResumeAppendableObjectUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>>
  StartUnbufferedUpload(UploadParams p) override {
    // TODO(#15114) - implement logging for writer connections
    return child_->StartUnbufferedUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>> StartBufferedUpload(
      UploadParams p) override {
    // TODO(#15114) - implement logging for writer connections
    return child_->StartBufferedUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>>
  ResumeUnbufferedUpload(ResumeUploadParams p) override {
    // TODO(#15114) - implement logging for writer connections
    return child_->ResumeUnbufferedUpload(std::move(p));
  }

  future<StatusOr<std::unique_ptr<AsyncWriterConnection>>> ResumeBufferedUpload(
      ResumeUploadParams p) override {
    // TODO(#15114) - implement logging for writer connections
    return child_->ResumeBufferedUpload(std::move(p));
  }

  future<StatusOr<google::storage::v2::Object>> ComposeObject(
      ComposeObjectParams p) override {
    GCP_LOG(INFO) << "ComposeObject("
                  << "bucket=" << p.request.destination().bucket()
                  << ", object=" << p.request.destination().name() << ")";
    auto fut = child_->ComposeObject(std::move(p));
    return fut.then([](auto f) {
      auto result = f.get();
      if (!result.ok()) {
        GCP_LOG(ERROR) << "ComposeObject failed: " << result.status();
      } else {
        GCP_LOG(INFO) << "ComposeObject succeeded";
      }
      return result;
    });
  }

  future<Status> DeleteObject(DeleteObjectParams p) override {
    GCP_LOG(INFO) << "DeleteObject("
                  << "bucket=" << p.request.bucket()
                  << ", object=" << p.request.object() << ")";
    auto fut = child_->DeleteObject(std::move(p));
    return fut.then([](auto f) {
      auto result = f.get();
      if (!result.ok()) {
        GCP_LOG(ERROR) << "DeleteObject failed: " << result;
      } else {
        GCP_LOG(INFO) << "DeleteObject succeeded";
      }
      return result;
    });
  }

  std::shared_ptr<AsyncRewriterConnection> RewriteObject(
      RewriteObjectParams p) override {
    // TODO(#15114) - implement logging for rewriter connections
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