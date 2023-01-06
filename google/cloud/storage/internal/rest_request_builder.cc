// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/rest_request_builder.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

RestRequestBuilder::RestRequestBuilder(std::string path)
    : request_(google::cloud::rest_internal::RestRequest(std::move(path))) {}

RestRequestBuilder& RestRequestBuilder::AddOption(CustomHeader const& p) {
  if (p.has_value()) {
    request_.AddHeader(p.custom_header_name(), p.value());
  }
  return *this;
}

RestRequestBuilder& RestRequestBuilder::AddOption(EncryptionKey const& p) {
  if (p.has_value()) {
    request_.AddHeader(std::string(EncryptionKey::prefix()) + "algorithm",
                       p.value().algorithm);
    request_.AddHeader(std::string(EncryptionKey::prefix()) + "key",
                       p.value().key);
    request_.AddHeader(std::string(EncryptionKey::prefix()) + "key-sha256",
                       p.value().sha256);
  }
  return *this;
}

RestRequestBuilder& RestRequestBuilder::AddOption(
    SourceEncryptionKey const& p) {
  if (p.has_value()) {
    request_.AddHeader(std::string(SourceEncryptionKey::prefix()) + "Algorithm",
                       p.value().algorithm);
    request_.AddHeader(std::string(SourceEncryptionKey::prefix()) + "Key",
                       p.value().key);
    request_.AddHeader(
        std::string(SourceEncryptionKey::prefix()) + "Key-Sha256",
        p.value().sha256);
  }
  return *this;
}

RestRequestBuilder& RestRequestBuilder::AddQueryParameter(
    std::string const& key, std::string const& value) {
  request_.AddQueryParameter(key, value);
  return *this;
}

RestRequestBuilder& RestRequestBuilder::AddHeader(std::string const& key,
                                                  std::string const& value) {
  request_.AddHeader(key, value);
  return *this;
}

google::cloud::rest_internal::RestRequest
RestRequestBuilder::BuildRequest() && {
  return std::move(request_);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
