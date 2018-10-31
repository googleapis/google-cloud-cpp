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
    : Client(std::shared_ptr<internal::RawClient>(
          new internal::CurlClient(std::move(options)))) {}

ObjectMetadata Client::UploadFileImpl(
    std::string const& file_name,
    internal::InsertObjectStreamingRequest request) {
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

  std::ifstream is(file_name);
  if (not is.is_open()) {
    std::string msg = __func__;
    msg += ": cannot open source file ";
    msg += file_name;
    google::cloud::internal::RaiseRuntimeError(msg);
  }
  if (not request.HasOption<DisableMD5Hash>() and
      not request.HasOption<MD5HashValue>()) {
    // Open a separate stream to read the file, because once we hit EOF there
    // is no guarantee we can rewind the file to the beginning.
    std::ifstream is2(file_name);
    MD5_CTX md5;
    MD5_Init(&md5);
    std::string buffer;
    buffer.resize(raw_client_->client_options().upload_buffer_size(), '\0');
    while (not is2.eof()) {
      is2.read(&buffer[0], buffer.size());
      MD5_Update(&md5, &buffer[0], static_cast<std::size_t>(is2.gcount()));
    }

    std::string hash(MD5_DIGEST_LENGTH, ' ');
    MD5_Final(reinterpret_cast<unsigned char*>(&hash[0]), &md5);
    request.set_option(
        MD5HashValue(internal::OpenSslUtils::Base64Encode(hash)));
  }

  auto result = raw_client_->WriteObject(request);
  auto streambuf = std::move(result.second);

  std::string buffer;
  buffer.resize(raw_client_->client_options().upload_buffer_size(), '\0');
  while (not is.eof() and is.good()) {
    is.read(&buffer[0], buffer.size());
    streambuf->sputn(&buffer[0], is.gcount());
  }
  auto response = streambuf->Close();
  if (response.status_code >= 300) {
    std::ostringstream os;
    os << __func__ << ": error in during upload "
       << Status(response.status_code, response.payload);
    google::cloud::internal::RaiseRuntimeError(os.str());
  }
  if (response.payload.empty()) {
    streambuf->ValidateHash(ObjectMetadata());
    return ObjectMetadata();
  }
  auto metadata = ObjectMetadata::ParseFromString(response.payload);
  streambuf->ValidateHash(metadata);
  return metadata;
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
