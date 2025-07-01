// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [all] [START storage_enable_otel_tracing]
#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/opentelemetry_options.h"
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <bucket-name> <project-id>\n";
    return 1;
  }
  std::string const bucket_name = argv[1];
  std::string const project_id = argv[2];

  // Create aliases to make the code easier to read.
  namespace gc = ::google::cloud;
  namespace gcs = ::google::cloud::storage;

  // Instantiate a basic tracing configuration which exports traces to Cloud
  // Trace. By default, spans are sent in batches and always sampled.
  auto project = gc::Project(project_id);
  auto configuration = gc::otel::ConfigureBasicTracing(project);

  // Create a client with OpenTelemetry tracing enabled.
  auto options = gc::Options{}.set<gc::OpenTelemetryTracingOption>(true);
  auto client = gcs::Client(options);

  auto writer = client.WriteObject(bucket_name, "quickstart.txt");
  writer << "Hello World!";
  writer.Close();
  if (!writer.metadata()) {
    std::cerr << "Error creating object: " << writer.metadata().status()
              << "\n";
    return 1;
  }
  std::cout << "Successfully created object: " << *writer.metadata() << "\n";

  auto reader = client.ReadObject(bucket_name, "quickstart.txt");
  if (!reader) {
    std::cerr << "Error reading object: " << reader.status() << "\n";
    return 1;
  }

  std::string contents{std::istreambuf_iterator<char>{reader}, {}};
  std::cout << contents << "\n";

  // The basic tracing configuration object goes out of scope. The collected
  // spans are flushed to Cloud Trace.

  return 0;
}
//! [all] [END storage_enable_otel_tracing]