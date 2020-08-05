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

#include "generator/generator.h"
#include "google/cloud/status.h"
#include "generator/internal/service_generator.h"
#include <google/api/client.pb.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace generator {

bool Generator::Generate(google::protobuf::FileDescriptor const* file,
                         std::string const&,
                         google::protobuf::compiler::GeneratorContext* context,
                         std::string* error) const {
  if (file->options().cc_generic_services()) {
    *error =
        "cpp codegen proto compiler plugin does not work with generic "
        "services. To generate cpp codegen APIs, please set \""
        "cc_generic_service = false\".";
    return false;
  }

  // TODO(sdhart): process any command line arguments

  std::vector<internal::ServiceGenerator> services;
  services.reserve(file->service_count());
  for (int i = 0; i < file->service_count(); i++) {
    services.emplace_back(file->service(i), context);
  }

  for (auto const& service : services) {
    auto result = service.Generate();
    if (!result.ok()) {
      *error = result.message();
      return false;
    }
  }

  return true;
}

}  // namespace generator
}  // namespace cloud
}  // namespace google