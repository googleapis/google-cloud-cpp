// Copyright 2023 Google LLC
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

#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/example_driver.h"
#include <opentelemetry/trace/provider.h>
#include <string>
#include <vector>

namespace {
using ::google::cloud::internal::GetEnv;

void MyApplicationCode() {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("gcloud-cpp/otel-samples");
  auto span = tracer->StartSpan("otel-samples");
  span->End();
}

void BasicTracingRate(std::vector<std::string> const& argv) {
  //! [otel-basic-tracing-rate]
  namespace gc = ::google::cloud;
  namespace otel = ::google::cloud::otel;
  [](std::string project_id) {
    auto project = gc::Project(std::move(project_id));
    auto options = gc::Options{}.set<otel::BasicTracingRateOption>(.001);
    auto configuration = gc::otel::ConfigureBasicTracing(project, options);

    MyApplicationCode();
  }
  //! [otel-basic-tracing-rate]
  (argv.at(0));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();

  std::cout << "\nRunning BasicTracingRate() sample" << std::endl;
  BasicTracingRate({project_id});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"basic-tracing-rate", BasicTracingRate},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
