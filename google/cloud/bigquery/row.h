// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_ROW_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_ROW_H

#include "google/cloud/bigquery/version.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
// TODO(aryann): Move all of the classes defined here except Client to their own
// files.

// TODO(aryann): Add an implementation for a row. We must support schemas that
// are known at compile-time as well as those that are known at run-time.
class Row {
 public:
  Row() = default;

  ~Row() = default;

  Row(Row const&) = default;
  Row& operator=(Row const&) = default;
  Row(Row&&) = default;
  Row& operator=(Row&&) = default;
};

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_ROW_H
