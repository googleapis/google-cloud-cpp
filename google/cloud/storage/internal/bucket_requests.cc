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

#include "google/cloud/storage/internal/bucket_requests.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os, ListBucketsRequest const& r) {
  os << "ListBucketsRequest={project_id=" << r.project_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ListBucketsResponse ListBucketsResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto json = storage::internal::nl::json::parse(response.payload);

  ListBucketsResponse result;
  result.next_page_token = json.value("nextPageToken", "");

  for (auto const& kv : json["items"].items()) {
    result.items.emplace_back(BucketMetadata::ParseFromJson(kv.value()));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListBucketsResponse const& r) {
  os << "ListBucketsResponse={next_page_token=" << r.next_page_token
     << ", items={";
  std::copy(r.items.begin(), r.items.end(),
            std::ostream_iterator<BucketMetadata>(os, "\n  "));
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os, GetBucketMetadataRequest const& r) {
  os << "GetBucketMetadataRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, CreateBucketRequest const& r) {
  os << "CreateBucketRequest={project_id=" << r.project_id()
     << ", metadata=" << r.metadata();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteBucketRequest const& r) {
  os << "DeleteBucketRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, UpdateBucketRequest const& r) {
  os << "UpdateBucketRequest={metadata=" << r.metadata();
  r.DumpOptions(os, ", ");
  return os << "}";
}

PatchBucketRequest::PatchBucketRequest(std::string bucket,
                                       BucketMetadata const& original,
                                       BucketMetadata const& updated)
    : bucket_(std::move(bucket)) {
  // Compare each modifiable field to build the patch
  BucketMetadataPatchBuilder builder;

  if (original.acl() != updated.acl()) {
    builder.SetAcl(updated.acl());
  }

  if (original.billing_as_optional() != updated.billing_as_optional()) {
    if (updated.has_billing()) {
      builder.SetBilling(updated.billing());
    } else {
      builder.ResetBilling();
    }
  }

  if (original.cors() != updated.cors()) {
    builder.SetCors(updated.cors());
  }

  if (original.default_acl() != updated.default_acl()) {
    builder.SetDefaultAcl(updated.default_acl());
  }

  if (original.encryption_as_optional() != updated.encryption_as_optional()) {
    if (updated.has_encryption()) {
      builder.SetEncryption(updated.encryption());
    } else {
      builder.ResetEncryption();
    }
  }

  if (original.labels() != updated.labels()) {
    if (updated.labels().empty()) {
      builder.ResetLabels();
    } else {
      std::map<std::string, std::string> difference;
      // Find the keys in the original map that are not in the new map:
      std::set_difference(original.labels().begin(), original.labels().end(),
                          updated.labels().begin(), updated.labels().end(),
                          std::inserter(difference, difference.end()),
                          // We want to compare just keys and ignore values, the
                          // map class provides such a function, so use it:
                          original.labels().value_comp());
      for (auto&& d : difference) {
        builder.ResetLabel(d.first);
      }

      // Find the elements (comparing key and value) in the updated map that
      // are not in the original map:
      difference.clear();
      std::set_difference(updated.labels().begin(), updated.labels().end(),
                          original.labels().begin(), original.labels().end(),
                          std::inserter(difference, difference.end()));
      for (auto&& d : difference) {
        builder.SetLabel(d.first, d.second);
      }
    }
  }

  if (original.lifecycle_as_optional() != updated.lifecycle_as_optional()) {
    if (updated.has_lifecycle()) {
      builder.SetLifecycle(updated.lifecycle());
    } else {
      builder.ResetLifecycle();
    }
  }

  if (original.location() != updated.location()) {
    builder.SetLocation(updated.location());
  }

  if (original.logging_as_optional() != updated.logging_as_optional()) {
    if (updated.has_logging()) {
      builder.SetLogging(updated.logging());
    } else {
      builder.ResetLogging();
    }
  }

  if (original.name() != updated.name()) {
    builder.SetName(updated.name());
  }

  if (original.storage_class() != updated.storage_class()) {
    builder.SetStorageClass(updated.storage_class());
  }

  if (original.versioning() != updated.versioning()) {
    if (updated.has_versioning()) {
      builder.SetVersioning(*updated.versioning());
    } else {
      builder.ResetVersioning();
    }
  }

  if (original.website_as_optional() != updated.website_as_optional()) {
    if (updated.has_website()) {
      builder.SetWebsite(updated.website());
    } else {
      builder.ResetWebsite();
    }
  }

  payload_ = builder.BuildPatch();
}

PatchBucketRequest::PatchBucketRequest(std::string bucket,
                                       BucketMetadataPatchBuilder const& patch)
    : bucket_(std::move(bucket)), payload_(patch.BuildPatch()) {}

std::ostream& operator<<(std::ostream& os, PatchBucketRequest const& r) {
  os << "PatchBucketRequest={bucket_name=" << r.bucket();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.payload() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
