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

#include "google/cloud/storage/object.h"
#include "google/cloud/internal/throw_delegate.h"
#include <sstream>
#include <thread>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
static_assert(std::is_copy_constructible<storage::Object>::value,
              "storage::Object must be copy-constructible");
static_assert(std::is_copy_assignable<storage::Object>::value,
              "storage::Objects must be copy-assignable");

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
