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
#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"
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

ObjectReadStream Client::ReadObjectImpl(
    internal::ReadObjectRangeRequest const& request) {
  auto source = raw_client_->ReadObject(request);
  if (!source) {
    ObjectReadStream error_stream(
        absl::make_unique<internal::ObjectReadStreambuf>(
            request, std::move(source).status()));
    error_stream.setstate(std::ios::badbit | std::ios::eofbit);
    return error_stream;
  }
  auto stream =
      ObjectReadStream(absl::make_unique<internal::ObjectReadStreambuf>(
          request, *std::move(source)));
  (void)stream.peek();
#if !GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Without exceptions the streambuf cannot report errors, so we have to
  // manually update the status bits.
  if (!stream.status().ok()) {
    stream.setstate(std::ios::badbit | std::ios::eofbit);
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  return stream;
}

ObjectWriteStream Client::WriteObjectImpl(
    internal::ResumableUploadRequest const& request) {
  auto session = raw_client_->CreateResumableSession(request);
  if (!session) {
    auto error = absl::make_unique<internal ::ResumableUploadSessionError>(
        std::move(session).status());

    ObjectWriteStream error_stream(
        absl::make_unique<internal::ObjectWriteStreambuf>(
            std::move(error), 0,
            absl::make_unique<internal::NullHashValidator>()));
    error_stream.setstate(std::ios::badbit | std::ios::eofbit);
    error_stream.Close();
    return error_stream;
  }
  return ObjectWriteStream(absl::make_unique<internal::ObjectWriteStreambuf>(
      *std::move(session), raw_client_->client_options().upload_buffer_size(),
      internal::CreateHashValidator(request)));
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
  std::ifstream is(file_name, std::ios::binary);
  if (!is.is_open()) {
    std::ostringstream os;
    os << __func__ << "(" << request << ", " << file_name
       << "): cannot open upload file source";
    return Status(StatusCode::kNotFound, std::move(os).str());
  }

  std::string payload(std::istreambuf_iterator<char>{is}, {});
  request.set_contents(std::move(payload));

  return raw_client_->InsertObjectMedia(request);
}

StatusOr<ObjectMetadata> Client::UploadFileResumable(
    std::string const& file_name,
    google::cloud::storage::internal::ResumableUploadRequest request) {
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
  } else {
    std::error_code size_err;
    auto file_size = google::cloud::internal::file_size(file_name, size_err);
    if (size_err) {
      return Status(StatusCode::kNotFound, size_err.message());
    }
    request.set_option(UploadContentLength(file_size));
  }
  std::ifstream source(file_name, std::ios::binary);
  if (!source.is_open()) {
    std::ostringstream os;
    os << __func__ << "(" << request << ", " << file_name
       << "): cannot open upload file source";
    return Status(StatusCode::kNotFound, std::move(os).str());
  }
  return UploadStreamResumable(source, request);
}

StatusOr<ObjectMetadata> Client::UploadStreamResumable(
    std::istream& source, internal::ResumableUploadRequest const& request) {
  StatusOr<std::unique_ptr<internal::ResumableUploadSession>> session_status =
      raw_client()->CreateResumableSession(request);
  if (!session_status) {
    return std::move(session_status).status();
  }

  auto session = std::move(*session_status);
  source.seekg(session->next_expected_byte(), std::ios::beg);

  // GCS requires chunks to be a multiple of 256KiB.
  auto chunk_size = internal::UploadChunkRequest::RoundUpToQuantum(
      raw_client()->client_options().upload_buffer_size());

  StatusOr<internal::ResumableUploadResponse> upload_response(
      internal::ResumableUploadResponse{});
  // We iterate while `source` is good and the retry policy has not been
  // exhausted.
  while (!source.eof() && upload_response &&
         !upload_response->payload.has_value()) {
    // Read a chunk of data from the source file.
    std::string buffer(chunk_size, '\0');
    source.read(&buffer[0], buffer.size());
    auto gcount = static_cast<std::size_t>(source.gcount());
    bool final_chunk = (gcount < buffer.size());
    auto source_size = session->next_expected_byte() + gcount;
    buffer.resize(gcount);

    auto expected = session->next_expected_byte() + gcount;
    if (final_chunk) {
      upload_response = session->UploadFinalChunk(buffer, source_size);
    } else {
      upload_response = session->UploadChunk(buffer);
    }
    if (!upload_response) {
      return std::move(upload_response).status();
    }
    if (session->next_expected_byte() != expected) {
      // Defensive programming: unless there is a bug, this should be dead code.
      return Status(
          StatusCode::kInternal,
          "Unexpected last committed byte expected=" +
              std::to_string(expected) +
              " got=" + std::to_string(session->next_expected_byte()) +
              ". This is a bug, please report it at "
              "https://github.com/googleapis/google-cloud-cpp/issues/new");
    }
  }

  if (!upload_response) {
    return std::move(upload_response).status();
  }

  return *std::move(upload_response->payload);
}

Status Client::DownloadFileImpl(internal::ReadObjectRangeRequest const& request,
                                std::string const& file_name) {
  auto report_error = [&request, file_name](char const* func, char const* what,
                                            Status const& status) {
    std::ostringstream msg;
    msg << func << "(" << request << ", " << file_name << "): " << what
        << " - status.message=" << status.message();
    return Status(status.code(), std::move(msg).str());
  };

  auto stream = ReadObjectImpl(request);
  if (!stream.status().ok()) {
    return report_error(__func__, "cannot open download source object",
                        stream.status());
  }

  // Open the destination file, and immediate raise an exception on failure.
  std::ofstream os(file_name, std::ios::binary);
  if (!os.is_open()) {
    return report_error(
        __func__, "cannot open download destination file",
        Status(StatusCode::kInvalidArgument, "ofstream::open()"));
  }

  std::string buffer;
  buffer.resize(raw_client_->client_options().download_buffer_size(), '\0');
  do {
    stream.read(&buffer[0], buffer.size());
    os.write(buffer.data(), stream.gcount());
  } while (os.good() && stream.good());
  os.close();
  if (!os.good()) {
    return report_error(__func__, "cannot close download destination file",
                        Status(StatusCode::kUnknown, "ofstream::close()"));
  }
  if (!stream.status().ok()) {
    return report_error(__func__, "error reading download source object",
                        stream.status());
  }
  return Status();
}

std::string Client::SigningEmail(SigningAccount const& signing_account) {
  if (signing_account.has_value()) {
    return signing_account.value();
  }
  return raw_client()->client_options().credentials()->AccountEmail();
}

StatusOr<Client::SignBlobResponseRaw> Client::SignBlobImpl(
    SigningAccount const& signing_account, std::string const& string_to_sign) {
  auto credentials = raw_client()->client_options().credentials();

  std::string signing_account_email = SigningEmail(signing_account);
  // First try to sign locally.
  auto signed_blob = credentials->SignBlob(signing_account, string_to_sign);
  if (signed_blob) {
    return SignBlobResponseRaw{credentials->KeyId(), *std::move(signed_blob)};
  }

  // If signing locally fails that may be because the credentials do not
  // support signing, or because the signing account is different than the
  // credentials account. In either case, try to sign using the API.
  internal::SignBlobRequest sign_request(
      signing_account_email, internal::Base64Encode(string_to_sign), {});
  auto response = raw_client()->SignBlob(sign_request);
  if (!response) {
    return response.status();
  }
  return SignBlobResponseRaw{response->key_id,
                             internal::Base64Decode(response->signed_blob)};
}

StatusOr<std::string> Client::SignUrlV2(
    internal::V2SignUrlRequest const& request) {
  SigningAccount const& signing_account = request.signing_account();
  auto signed_blob = SignBlobImpl(signing_account, request.StringToSign());
  if (!signed_blob) {
    return signed_blob.status();
  }

  internal::CurlHandle curl;
  auto encoded = internal::Base64Encode(signed_blob->signed_blob);
  std::string signature = curl.MakeEscapedString(encoded).get();

  std::ostringstream os;
  os << "https://storage.googleapis.com/" << request.bucket_name();
  if (!request.object_name().empty()) {
    os << '/' << curl.MakeEscapedString(request.object_name()).get();
  }
  os << "?GoogleAccessId=" << SigningEmail(signing_account)
     << "&Expires=" << request.expiration_time_as_seconds().count()
     << "&Signature=" << signature;

  return std::move(os).str();
}

StatusOr<std::string> Client::SignUrlV4(internal::V4SignUrlRequest request) {
  auto valid = request.Validate();
  if (!valid.ok()) {
    return valid;
  }
  request.AddMissingRequiredHeaders();
  SigningAccount const& signing_account = request.signing_account();
  auto signing_email = SigningEmail(signing_account);

  auto string_to_sign = request.StringToSign(signing_email);
  auto signed_blob = SignBlobImpl(signing_account, string_to_sign);
  if (!signed_blob) {
    return signed_blob.status();
  }

  std::string signature = internal::HexEncode(signed_blob->signed_blob);
  internal::CurlHandle curl;
  std::ostringstream os;
  os << request.HostnameWithBucket();
  for (auto& part : request.ObjectNameParts()) {
    os << '/' << curl.MakeEscapedString(part).get();
  }
  os << "?" << request.CanonicalQueryString(signing_email)
     << "&X-Goog-Signature=" << signature;

  return std::move(os).str();
}

StatusOr<PolicyDocumentResult> Client::SignPolicyDocument(
    internal::PolicyDocumentRequest const& request) {
  SigningAccount const& signing_account = request.signing_account();
  auto signing_email = SigningEmail(signing_account);

  auto string_to_sign = request.StringToSign();
  auto base64_policy = internal::Base64Encode(string_to_sign);
  auto signed_blob = SignBlobImpl(signing_account, base64_policy);
  if (!signed_blob) {
    return signed_blob.status();
  }

  return PolicyDocumentResult{
      signing_email, request.policy_document().expiration, base64_policy,
      internal::Base64Encode(signed_blob->signed_blob)};
}

StatusOr<PolicyDocumentV4Result> Client::SignPolicyDocumentV4(
    internal::PolicyDocumentV4Request request) {
  SigningAccount const& signing_account = request.signing_account();
  auto signing_email = SigningEmail(signing_account);
  request.SetSigningEmail(signing_email);

  auto string_to_sign = request.StringToSign();
  auto escaped = internal::PostPolicyV4Escape(string_to_sign);
  if (!escaped) return escaped.status();
  auto base64_policy = internal::Base64Encode(*escaped);
  auto signed_blob = SignBlobImpl(signing_account, base64_policy);
  if (!signed_blob) {
    return signed_blob.status();
  }
  std::string signature = internal::HexEncode(signed_blob->signed_blob);
  auto required_fiels = request.RequiredFormFields();
  required_fiels["x-goog-signature"] = signature;
  required_fiels["policy"] = base64_policy;
  return PolicyDocumentV4Result{request.Url(),
                                request.Credentials(),
                                request.ExpirationDate(),
                                base64_policy,
                                signature,
                                "GOOG4-RSA-SHA256",
                                std::move(required_fiels)};
}

std::string CreateRandomPrefixName(std::string const& prefix) {
  auto constexpr kPrefixNameSize = 16;
  auto rng = google::cloud::internal::MakeDefaultPRNG();
  return prefix + google::cloud::internal::Sample(rng, kPrefixNameSize,
                                                  "abcdefghijklmnopqrstuvwxyz");
}

namespace internal {

ScopedDeleter::ScopedDeleter(
    std::function<Status(std::string, std::int64_t)> delete_fun)
    : enabled_(true), delete_fun_(std::move(delete_fun)) {}

ScopedDeleter::~ScopedDeleter() {
  if (enabled_) {
    ExecuteDelete();
  }
}

void ScopedDeleter::Add(ObjectMetadata const& object) {
  auto generation = object.generation();
  Add(std::move(object).name(), generation);
}

void ScopedDeleter::Add(std::string object_name, std::int64_t generation) {
  object_list_.emplace_back(std::move(object_name), generation);
}

Status ScopedDeleter::ExecuteDelete() {
  std::vector<std::pair<std::string, std::int64_t>> object_list;
  // make sure the dtor will not do this again
  object_list.swap(object_list_);

  // Perform deletion in reverse order. We rely on it in functions which create
  // a "lock" object - it is created as the first file and should be removed as
  // last.
  for (auto object_it = object_list.rbegin(); object_it != object_list.rend();
       ++object_it) {
    Status status = delete_fun_(std::move(object_it->first), object_it->second);
    // Fail on first error. If the service is unavailable, every deletion
    // would potentially keep retrying until the timeout passes - this would
    // take way too much time and would be pointless.
    if (!status.ok()) {
      return status;
    }
  }
  return Status();
}

}  // namespace internal

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
