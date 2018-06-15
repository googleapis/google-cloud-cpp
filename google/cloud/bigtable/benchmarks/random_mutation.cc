// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/benchmarks/random_mutation.h"
#include "google/cloud/bigtable/benchmarks/constants.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
bigtable::Mutation MakeRandomMutation(google::cloud::internal::DefaultPRNG& gen,
                                      int f) {
  std::string field = "field" + std::to_string(f);
  return bigtable::SetCell(kColumnFamily, std::move(field),
                           std::chrono::milliseconds(0), MakeRandomValue(gen));
}

std::string MakeRandomValue(google::cloud::internal::DefaultPRNG& generator) {
  static std::string const letters(
      "ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijlkmnopqrstuvwxyz0123456789-/_");
  return google::cloud::internal::Sample(generator, kFieldSize, letters);
}
}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
