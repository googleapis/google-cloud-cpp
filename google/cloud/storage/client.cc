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
#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
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

std::shared_ptr<internal::RawClient> Client::CreateDefaultInternalClient(
    ClientOptions options) {
  return internal::CurlClient::Create(std::move(options));
}

StatusOr<Client> Client::CreateDefaultClient() {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  if (!opts) {
    return StatusOr<Client>(opts.status());
  }
  return StatusOr<Client>(Client(*opts));
}

bool Client::UseSimpleUpload(std::string const& file_name) const {
  auto status = google::cloud::internal::status(file_name);
  if (!is_regular(status)) {
    return false;
  }
  auto size = google::cloud::internal::file_size(file_name);
  return size <= raw_client()->client_options().maximum_simple_upload_size();
}

StatusOr<ObjectMetadata> Client::UploadFileSimple(
    std::string const& file_name, internal::InsertObjectMediaRequest request) {
  std::ifstream is(file_name);
  if (!is.is_open()) {
    std::string msg = __func__;
    msg += ": cannot open source file ";
    msg += file_name;
    google::cloud::internal::ThrowRuntimeError(msg);
  }

  std::string payload(std::istreambuf_iterator<char>{is}, {});
  request.set_contents(std::move(payload));

  return raw_client_->InsertObjectMedia(request);
}

StatusOr<ObjectMetadata> Client::UploadFileResumable(
    std::string const& file_name,
    google::cloud::storage::internal::ResumableUploadRequest const& request) {
  auto status = google::cloud::internal::status(file_name);
  if (!is_regular(status)) {
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
  if (!source.is_open()) {
    std::string msg = __func__;
    msg += ": cannot open source file ";
    msg += file_name;
    return Status(StatusCode::kNotFound, std::move(msg));
  }
  // This function only works for regular files, and the `storage::Client`
  // class checks before calling it.
  std::uint64_t source_size = google::cloud::internal::file_size(file_name);

  return UploadStreamResumable(source, source_size, request);
}

StatusOr<ObjectMetadata> Client::UploadStreamResumable(
    std::istream& source, std::uint64_t source_size,
    internal::ResumableUploadRequest const& request) {
  StatusOr<std::unique_ptr<internal::ResumableUploadSession>> session_status =
      raw_client()->CreateResumableSession(request);
  if (!session_status) {
    return std::move(session_status).status();
  }

  auto session = std::move(*session_status);

  // GCS requires chunks to be a multiple of 256KiB.
  auto chunk_size = internal::UploadChunkRequest::RoundUpToQuantum(
      raw_client()->client_options().upload_buffer_size());

  StatusOr<internal::ResumableUploadResponse> upload_response(
      internal::ResumableUploadResponse{});
  // We iterate while `source` is good and the retry policy has not been
  // exhausted.
  while (!source.eof() && upload_response && upload_response->payload.empty()) {
    // Read a chunk of data from the source file.
    std::string buffer(chunk_size, '\0');
    source.read(&buffer[0], buffer.size());
    auto gcount = static_cast<std::size_t>(source.gcount());
    if (gcount < buffer.size()) {
      source_size = session->next_expected_byte() + gcount;
    }
    buffer.resize(gcount);

    auto expected = session->next_expected_byte() + gcount - 1;
    upload_response = session->UploadChunk(buffer, source_size);
    if (!upload_response) {
      return std::move(upload_response).status();
    }
    if (session->next_expected_byte() != expected) {
      GCP_LOG(WARNING) << "unexpected last committed byte "
                       << " expected=" << expected
                       << " got=" << session->next_expected_byte();
      source.seekg(session->next_expected_byte(), std::ios::beg);
    }
  }

  if (!upload_response) {
    return std::move(upload_response).status();
  }

  return internal::ObjectMetadataParser::FromString(upload_response->payload);
}

Status Client::DownloadFileImpl(internal::ReadObjectRangeRequest const& request,
                                std::string const& file_name) {
  // TODO(#1665) - use Status to report errors.
  std::unique_ptr<internal::ObjectReadStreambuf> streambuf =
      raw_client_->ReadObject(request).value();
  // Open the download stream and immediately raise an exception on failure.
  ObjectReadStream stream(std::move(streambuf));

  auto report_error = [&](char const* func, char const* what) {
    std::ostringstream msg;
    msg << func << "(" << request << ", " << file_name << "): " << what
        << " - status.message=" << stream.status().message();
    return Status(stream.status().code(), std::move(msg).str());
  };
  if (!stream.status().ok()) {
    return report_error(__func__, "cannot open destination file");
  }

  // Open the destination file, and immediate raise an exception on failure.
  std::ofstream os(file_name);
  if (!os.is_open()) {
    std::ostringstream msg;
    msg << __func__ << "(" << request << ", " << file_name << "): "
        << "cannot open destination file";
    return Status(StatusCode::kInvalidArgument, std::move(msg).str());
  }

  std::string buffer;
  buffer.resize(raw_client_->client_options().download_buffer_size(), '\0');
  do {
    stream.read(&buffer[0], buffer.size());
    os.write(buffer.data(), stream.gcount());
  } while (os.good() && stream.good());
  os.close();
  if (!os.good()) {
    std::ostringstream msg;
    msg << __func__ << "(" << request << ", " << file_name << "): "
        << "cannot close destination file";
    return Status(StatusCode::kUnknown, std::move(msg).str());
  }
  if (!stream.status().ok()) {
    return report_error(__func__, "error in download stream");
  }
  return Status();
}

StatusOr<std::string> Client::SignUrl(internal::SignUrlRequest const& request) {
  auto base_credentials = raw_client()->client_options().credentials();
  auto credentials = dynamic_cast<oauth2::ServiceAccountCredentials<>*>(
      base_credentials.get());

  if (credentials == nullptr) {
    return Status(StatusCode::kInvalidArgument,
                  R"""(The current credentials cannot be used to sign URLs.
Please configure your google::cloud::storage::Client to use service account
credentials, as described in:
https://cloud.google.com/storage/docs/authentication
)""");
  }

  auto result = credentials->SignString(request.StringToSign());
  if (!result.first.ok()) {
    return result.first;
  }

  internal::CurlHandle curl;
  std::string signature = curl.MakeEscapedString(result.second).get();

  std::ostringstream os;
  os << "https://storage.googleapis.com/" << request.bucket_name();
  if (!request.object_name().empty()) {
    os << '/' << curl.MakeEscapedString(request.object_name()).get();
  }
  os << "?GoogleAccessId=" << credentials->client_id()
     << "&Expires=" << request.expiration_time_as_seconds().count()
     << "&Signature=" << signature;

  return std::move(os).str();
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
