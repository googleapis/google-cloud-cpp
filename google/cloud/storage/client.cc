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

#include "google/cloud/storage/client.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include <openssl/md5.h>
#include <fstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
static_assert(std::is_copy_constructible<storage::Client>::value,
              "storage::Client must be constructible");
static_assert(std::is_copy_assignable<storage::Client>::value,
              "storage::Client must be assignable");

Client::Client(ClientOptions options)
    : Client(internal::CurlClient::Create(std::move(options))) {}

bool Client::UseSimpleUpload(std::string const& file_name) const {
  auto status = google::cloud::internal::status(file_name);
  if (not is_regular(status)) {
    return false;
  }
  auto size = google::cloud::internal::file_size(file_name);
  return size <= raw_client()->client_options().maximum_simple_upload_size();
}

ObjectMetadata Client::UploadFileSimple(
    std::string const& file_name, internal::InsertObjectMediaRequest request) {
  std::ifstream is(file_name);
  if (not is.is_open()) {
    std::string msg = __func__;
    msg += ": cannot open source file ";
    msg += file_name;
    google::cloud::internal::RaiseRuntimeError(msg);
  }

  std::string payload(std::istreambuf_iterator<char>{is}, {});
  request.set_contents(std::move(payload));

  return raw_client_->InsertObjectMedia(request).second;
}

ObjectMetadata Client::UploadFileResumable(
    std::string const& file_name,
    google::cloud::storage::internal::ResumableUploadRequest const& request) {
  auto status = google::cloud::internal::status(file_name);
  if (not is_regular(status)) {
    GCP_LOG(WARNING) << "Trying to upload " << file_name
                     << R"""( which is not a regular file.
This is often a problem because:
  - Some non-regular files are infinite sources of data, and the load will
    never complete.
  - Some non-regular files can only be read once, and UploadFile() may need to
    read the file more than once to compute the checksum and hashes needed to
    preserve data integrity.

Consider using Client::WriteObject() instead. You may also need to disable data
integrity checks using the DisableMD5Hash() and DisableCrc32cChecksum() options.
)""";
  }

  std::ifstream source(file_name);
  if (not source.is_open()) {
    std::string msg = __func__;
    msg += ": cannot open source file ";
    msg += file_name;
    google::cloud::internal::RaiseRuntimeError(msg);
  }
  // This function only works for regular files, and the `storage::Client()`
  // class checks before calling it.
  std::uint64_t source_size = google::cloud::internal::file_size(file_name);

  auto result = UploadStreamResumable(source, source_size, request);
  if (not result.first.ok()) {
    google::cloud::internal::RaiseRuntimeError(result.first.error_message());
  }
  return result.second;
}

std::pair<Status, ObjectMetadata> Client::UploadStreamResumable(
    std::istream& source, std::uint64_t source_size,
    internal::ResumableUploadRequest const& request) {
  Status status;
  std::unique_ptr<internal::ResumableUploadSession> session;
  std::tie(status, session) = raw_client()->CreateResumableSession(request);
  if (not status.ok()) {
    return std::make_pair(status, ObjectMetadata{});
  }

  // GCS requires chunks to be a multiple of 256KiB.
  auto chunk_size = internal::UploadChunkRequest::RoundUpToQuantum(
      raw_client()->client_options().upload_buffer_size());

  internal::ResumableUploadResponse upload_response;
  // We iterate while `source` is good and the retry policy has not been
  // exhausted.
  while (not source.eof() and upload_response.payload.empty()) {
    // Read a chunk of data from the source file.
    std::string buffer(chunk_size, '\0');
    source.read(&buffer[0], buffer.size());
    auto gcount = static_cast<std::size_t>(source.gcount());
    if (gcount < buffer.size()) {
      source_size = session->next_expected_byte() + gcount;
    }
    buffer.resize(gcount);

    auto expected = session->next_expected_byte() + gcount - 1;
    std::tie(status, upload_response) =
        session->UploadChunk(buffer, source_size);
    if (not status.ok()) {
      return std::make_pair(Status(), ObjectMetadata());
    }
    if (session->next_expected_byte() != expected) {
      GCP_LOG(WARNING) << "unexpected last committed byte "
                       << " expected=" << expected
                       << " got=" << session->next_expected_byte();
      source.seekg(session->next_expected_byte(), std::ios::beg);
    }
  }

  if (not status.ok()) {
    return std::make_pair(status, ObjectMetadata{});
  }

  return std::make_pair(
      Status(), ObjectMetadata::ParseFromString(upload_response.payload));
}

void Client::DownloadFileImpl(internal::ReadObjectRangeRequest const& request,
                              std::string const& file_name) {
  std::unique_ptr<internal::ObjectReadStreambuf> streambuf =
      raw_client_->ReadObject(request).second;
  ObjectReadStream stream(std::move(streambuf));
  if (stream.eof() and not stream.IsOpen()) {
    std::string msg = __func__;
    msg += ": cannot open source object ";
    msg += request.object_name();
    msg += " in bucket ";
    msg += request.bucket_name();
    google::cloud::internal::RaiseRuntimeError(msg);
  }
  std::ofstream os(file_name);
  if (not os.is_open()) {
    std::string msg = __func__;
    msg += ": cannot open destination file ";
    msg += file_name;
    google::cloud::internal::RaiseRuntimeError(msg);
  }
  std::string buffer;
  buffer.resize(raw_client_->client_options().download_buffer_size(), '\0');
  do {
    stream.read(&buffer[0], buffer.size());
    os.write(buffer.data(), stream.gcount());
    if (not stream.good()) {
      break;
    }
  } while (os.good());
  os.close();
  if (not os.good()) {
    std::string msg = __func__;
    msg += ": failure closing destination file ";
    msg += file_name;
    google::cloud::internal::RaiseRuntimeError(msg);
  }
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
