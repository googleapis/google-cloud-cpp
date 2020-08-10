// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_CLASS_GENERATOR_INTERFACE_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_CLASS_GENERATOR_INTERFACE_H

#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Interface class such that ServiceGenerator can invoke its collection of
 * class generators polymorphically.
 */
class ClassGeneratorInterface {
 public:
  virtual ~ClassGeneratorInterface() = default;
  virtual Status Generate() = 0;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_CLASS_GENERATOR_INTERFACE_H
