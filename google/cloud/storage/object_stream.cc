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

#include "google/cloud/storage/object_stream.h"
#include "google/cloud/log.h"
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
static_assert(std::is_move_assignable<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move assignable.");
static_assert(std::is_move_constructible<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move constructible.");

ObjectReadStream::~ObjectReadStream() {
  if (not IsOpen()) {
    return;
  }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
    Close();
  } catch (std::exception const& ex) {
    GCP_LOG(INFO) << "Ignored exception while trying to close stream: "
                  << ex.what();
  } catch (...) {
    GCP_LOG(INFO) << "Ignored unknown exception while trying to close stream";
  }
#else
  Close();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

internal::HttpResponse ObjectReadStream::Close() {
  if (not IsOpen()) {
    google::cloud::internal::RaiseRuntimeError(
        "Attempting to Close() closed ObjectReadStream");
  }
  return buf_->Close();
}

ObjectWriteStream::~ObjectWriteStream() {
  if (not IsOpen()) {
    return;
  }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
    CloseRaw();
  } catch (std::exception const& ex) {
    GCP_LOG(INFO) << "Ignored exception while trying to close stream: "
                  << ex.what();
  } catch (...) {
    GCP_LOG(INFO) << "Ignored unknown exception while trying to close stream";
  }
#else
  CloseRaw();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

internal::HttpResponse ObjectWriteStream::CloseRaw() {
  if (not IsOpen()) {
    google::cloud::internal::RaiseRuntimeError(
        "Attempting to Close() closed ObjectWriteStream");
  }
  return buf_->Close();
}

ObjectMetadata ObjectWriteStream::Close() {
  auto response = CloseRaw();
  if (response.status_code >= 300) {
    std::ostringstream os;
    os << "Error in " << __func__ << ": "
       << Status(response.status_code, response.payload);
    google::cloud::internal::RaiseRuntimeError(os.str());
  }
  if (response.payload.empty()) {
    buf_->ValidateHash(ObjectMetadata());
    return ObjectMetadata();
  }
  auto metadata = ObjectMetadata::ParseFromString(response.payload);
  buf_->ValidateHash(metadata);
  return metadata;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
