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

#include "google/cloud/storage/storage_class.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace storage_class {
std::string const& Standard() {
  static std::string const kStorageClass = "STANDARD";
  return kStorageClass;
}

std::string const& MultiRegional() {
  static std::string const kStorageClass = "MULTI_REGIONAL";
  return kStorageClass;
}

std::string const& Regional() {
  static std::string const kStorageClass = "REGIONAL";
  return kStorageClass;
}

std::string const& Nearline() {
  static std::string const kStorageClass = "NEARLINE";
  return kStorageClass;
}

std::string const& Coldline() {
  static std::string const kStorageClass = "COLDLINE";
  return kStorageClass;
}

std::string const& DurableReducedAvailability() {
  static std::string const kStorageClass = "DURABLE_REDUCED_AVAILABILITY";
  return kStorageClass;
}

}  // namespace storage_class
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
