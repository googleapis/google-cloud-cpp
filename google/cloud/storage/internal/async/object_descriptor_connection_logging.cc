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

#include "google/cloud/storage/internal/async/object_descriptor_connection_logging.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/internal/async/reader_connection_logging.h"
#include "google/cloud/log.h"
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage_experimental::AsyncReaderConnection;
using ::google::cloud::storage_experimental::ObjectDescriptorConnection;

class ObjectDescriptorConnectionLogging : public ObjectDescriptorConnection {
 public:
  explicit ObjectDescriptorConnectionLogging(
      std::shared_ptr<ObjectDescriptorConnection> child)
      : child_(std::move(child)) {}

  Options options() const override { return child_->options(); }

  absl::optional<google::storage::v2::Object> metadata() const override {
    return child_->metadata();
  }

  std::unique_ptr<AsyncReaderConnection> Read(ReadParams p) override {
    GCP_LOG(INFO) << "ObjectDescriptorConnection::Read called";
    auto conn = child_->Read(std::move(p));
    return MakeLoggingReaderConnection(options(), std::move(conn));
  }

 private:
  std::shared_ptr<ObjectDescriptorConnection> child_;
};

}  // namespace

std::shared_ptr<storage_experimental::ObjectDescriptorConnection>
MakeLoggingObjectDescriptorConnection(
    std::shared_ptr<storage_experimental::ObjectDescriptorConnection> impl) {
  return std::make_shared<ObjectDescriptorConnectionLogging>(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google