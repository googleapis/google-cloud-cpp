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

#ifndef GOOGLE_CLOUD_CPP_STORAGE_CLIENT_CLIENT_H_
#define GOOGLE_CLOUD_CPP_STORAGE_CLIENT_CLIENT_H_

#include "storage/client/bucket_metadata.h"
#include "storage/client/credentials.h"
#include "storage/client/status.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Define the interface used to communicate with Google Cloud Storage.
 *
 * This is a dependency injection point so higher-level abstractions (like
 * `storage::Bucket` or `storage::Object`) can be effectively tested.
 */
class Client {
 public:
  virtual ~Client() = default;

  // The member functions of this class are not intended for general use by
  // application developers (they are simply a dependency injection point). Make
  // them protected, so the mock classes can override them, and then make the
  // classes that do use them friends.
 protected:
  friend class Bucket;
  virtual std::pair<Status, BucketMetadata> BucketGet(
      std::string const& bucket_name) = 0;
};

/**
 * Create the default client for the Google Cloud Storage C++ Library.
 *
 * TODO(#549) - this function will need a set of ClientOptions.
 */
std::shared_ptr<Client> CreateDefaultClient(
    std::shared_ptr<Credentials> credentials);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_STORAGE_CLIENT_CLIENT_H_
